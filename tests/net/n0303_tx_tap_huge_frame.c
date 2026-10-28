/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0303: tx_tap_huge_frame
 *
 * Non GSO frames of 1, 4, 16 and 64 MiB (header + one data
 * descriptor, hdr_len 0xffff as a hint the device does not validate).
 * The host tun driver cannot build an skb for the larger ones and
 * writev() fails, for example with ENOBUFS (16 MiB, kmalloc too large) or
 * EMSGSIZE (frags above MAX_SKB_FRAGS * 32 KiB). The descriptor chain
 * itself is valid, so the device must drop such a frame like any
 * other undeliverable one: return it with used.len 0, and keep the
 * queue working without NEEDS_RESET.
 *
 * The data descriptor points at the start of the first guest RAM region
 * that holds the largest frame. TX only reads it, and the content is
 * irrelevant.
 */
#include "tests/test.h"

static uint16_t tx_aidx;

/* Publish head, kick and wait for one used entry. */
static test_result_t tx_post(struct virtio_dev *dev, struct vring *vr,
                             uint16_t head)
{
    vring_raw_set_avail(vr, tx_aidx % vr->size, head);
    tx_aidx++;
    vring_raw_set_avail_idx(vr, tx_aidx);
    return vv_kick_and_wait(dev, vr, 1, VV_TIMEOUT_MS);
}

static test_result_t test_net_tx_tap_huge_frame(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    static const uint32_t lens[] = {
        1u << 20,  /* 1 MiB */
        4u << 20,  /* 4 MiB */
        16u << 20, /* 16 MiB */
        64u << 20, /* 64 MiB */
    };
    const unsigned nlens = sizeof(lens) / sizeof(lens[0]);
    uint64_t big_addr;
    if (vv_find_ram_range(lens[nlens - 1], &big_addr) < 0)
        TSKIP("no guest RAM region of %u bytes", lens[nlens - 1]);

    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    /* A hint only. The device must not trust it. */
    hdr->hdr_len = 0xffff;

    for (unsigned i = 0; i < nlens; i++) {
        /* Each frame uses two descriptors: header and data. */
        uint16_t h = 2 * i;
        vv_desc(vr, h, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, h + 1);
        vring_raw_set_desc(vr, h + 1, big_addr, lens[i], 0, 0);

        VV_TRY(tx_post(dev, vr, h));
    }
    TSKIP("not implemented");
}

REGISTER_TEST(N0303, VIRTIO_PCI_DEVICE_NET, test_net_tx_tap_huge_frame,
              "TX frames so large the tap write fails",
              VIRTIO_SPEC_V1_2, "5.1.6.2");

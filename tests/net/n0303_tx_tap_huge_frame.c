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
 * The data descriptor points at guest RAM from 2 MiB on. TX only
 * reads it, and the content is irrelevant.
 */
#include "tests/test.h"

/* The probe uses the last two descriptor slots, the test the rest. */
#define PROBE_SLOT(vr) ((uint16_t)((vr)->size - 2))

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

/* The last used entry must return head with len 0 (nothing written). */
static int tx_check_used(struct vring *vr, uint16_t head)
{
    __sync_synchronize();
    uint16_t u = vr->used->idx;
    struct vring_used_elem *e = &vr->used->ring[(uint16_t)(u - 1) % vr->size];
    if (e->id != head || e->len != 0) {
        printf("vv-info used[%u]: id=%u len=%u, want id=%u len=0\n",
               (uint16_t)(u - 1), e->id, e->len, head);
        return -1;
    }
    return 0;
}

static int tx_needs_reset(struct virtio_dev *dev)
{
    __sync_synchronize();
    return (dev->common->device_status & VIRTIO_STATUS_NEEDS_RESET) != 0;
}

/* Minimum Ethernet frame length without FCS. */
#define ETH_MIN_LEN 60

/* Fill a minimal valid broadcast Ethernet frame of len bytes. */
static void tx_fill_frame(uint8_t *f, size_t len)
{
    memset(f, 0xff, 6);     /* destination: broadcast */
    memset(f + 6, 0x02, 6); /* source: locally administered */
    f[12] = 0x08;           /* EtherType IPv4 (0x0800) */
    f[13] = 0x00;
    memset(f + 14, 0xab, len - 14);
}

/*
 * Liveness probe: a well formed 2 descriptor frame must still complete
 * with len 0 and must not leave the device in NEEDS_RESET.
 */
static test_result_t tx_probe(struct virtio_dev *dev, struct vring *vr)
{
    static struct virtio_net_hdr_mrg *hdr;
    static uint8_t *frame;
    if (!hdr) {
        hdr = vv_alloc_pages(1);
        frame = vv_alloc_pages(1);
    }
    tx_fill_frame(frame, ETH_MIN_LEN);
    uint16_t slot = PROBE_SLOT(vr);
    vv_desc(vr, slot, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, slot + 1);
    vv_desc(vr, slot + 1, frame, ETH_MIN_LEN, 0, 0);
    test_result_t r = tx_post(dev, vr, slot);
    if (r != TEST_PASS)
        TFAIL("probe frame not completed (r=%d status=0x%x)", r,
              dev->common->device_status);
    if (tx_check_used(vr, slot))
        TFAIL("probe used entry wrong");
    if (tx_needs_reset(dev))
        TFAIL("NEEDS_RESET after probe");
    return TEST_PASS;
}

/* Guest RAM from 2 MiB on. TX only reads it, the content is irrelevant. */
#define BIG_ADDR 0x200000ull

static test_result_t test_net_tx_tap_huge_frame(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    static const uint32_t lens[] = {
        1u << 20,  /* 1 MiB */
        4u << 20,  /* 4 MiB */
        16u << 20, /* 16 MiB */
        64u << 20, /* 64 MiB */
    };
    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    /* A hint only. The device must not trust it. */
    hdr->hdr_len = 0xffff;

    for (unsigned i = 0; i < sizeof(lens) / sizeof(lens[0]); i++) {
        /* Each frame uses two descriptors: header and data. */
        uint16_t h = 2 * i;
        vv_desc(vr, h, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, h + 1);
        vring_raw_set_desc(vr, h + 1, BIG_ADDR, lens[i], 0, 0);

        /*
         * The tap refuses the larger frames. The device must still return
         * each chain with len 0 and must not set NEEDS_RESET.
         */
        test_result_t r = tx_post(dev, vr, h);
        if (r != TEST_PASS)
            TFAIL("%u byte frame never returned to used ring (r=%d "
                  "status=0x%x NEEDS_RESET=%d)", lens[i], r,
                  dev->common->device_status, tx_needs_reset(dev));
        if (tx_check_used(vr, h))
            TFAIL("bad used entry for %u byte frame", lens[i]);
        if (tx_needs_reset(dev))
            TFAIL("NEEDS_RESET after %u byte frame", lens[i]);
    }

    /* The queue must still transmit a normal frame afterwards. */
    return tx_probe(dev, vr);
}

REGISTER_TEST(N0303, VIRTIO_PCI_DEVICE_NET, test_net_tx_tap_huge_frame,
              "TX frames so large the tap write fails",
              VIRTIO_SPEC_V1_2, "5.1.6.2");

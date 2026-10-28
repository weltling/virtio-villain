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

static test_result_t test_net_tx_tap_huge_frame(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)dev;
    (void)vr;
    TSKIP("not implemented");
}

REGISTER_TEST(N0303, VIRTIO_PCI_DEVICE_NET, test_net_tx_tap_huge_frame,
              "TX frames so large the tap write fails",
              VIRTIO_SPEC_V1_2, "5.1.6.2");

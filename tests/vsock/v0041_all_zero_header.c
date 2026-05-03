/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0041: TX header with all-zero fields (spec 5.10.6)
 *
 * Send a vsock packet with all header fields set to zero.
 * This creates an invalid packet (op=0 is invalid, type=0 is invalid).
 * The device must not crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_vsock_hdr {
    uint64_t src_cid;
    uint64_t dst_cid;
    uint32_t src_port;
    uint32_t dst_port;
    uint32_t len;
    uint16_t type;
    uint16_t op;
    uint32_t flags;
    uint32_t buf_alloc;
    uint32_t fwd_cnt;
} __attribute__((packed));

static test_result_t test_vsock_all_zero_hdr(struct virtio_dev *dev,
                                             struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* TX on queue 1 */
    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    struct virtio_vsock_hdr *pkt = vv_alloc_pages(1);

    /* All zeros: src_cid=0, dst_cid=0, type=0, op=0, etc. */
    memset(pkt, 0, sizeof(*pkt));

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(pkt),
                       sizeof(*pkt), 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0041, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_all_zero_hdr,
              "TX vsock packet with all-zero header fields",
              VIRTIO_SPEC_V1_2, "5.10.6");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0006: net_mergeable_num_buffers
 *
 * Submit an RX buffer with a corrupted num_buffers field in the
 * mergeable receive buffer header area. The device must handle
 * this safely without reading beyond allocated buffers.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr_mrg {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));

static test_result_t test_net_mergeable_abuse(struct virtio_dev *dev,
                                              struct vring *vr)
{
    /*
     * Provide a single small RX buffer. If the device writes
     * num_buffers = 0xFFFF into it (or reads a corrupted value),
     * it should not try to chain 65535 buffers.
     */
    struct virtio_net_hdr_mrg *buf = vv_alloc_pages(1);
    memset(buf, 0, PAGE_SIZE);

    /* Pre-fill with garbage num_buffers to test device resilience */
    buf->num_buffers = 0xFFFF;

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* Single RX buffer (writable) */
    vring_raw_set_desc(vr, 0, buf_phys, PAGE_SIZE,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0006, VIRTIO_PCI_DEVICE_NET, test_net_mergeable_abuse,
              "Corrupted num_buffers in mergeable RX header",
              VIRTIO_SPEC_V1_2, "5.1.6", 0);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0057: RX buffer with INDIRECT|WRITE flags combined (spec 5.1.6.4)
 *
 * Post an RX buffer that has both INDIRECT and WRITE flags set on
 * the same descriptor. This is a legal combination but tests device
 * handling of indirect writable buffers in the receive path.
 */
#include "lib/virtio_test.h"

static test_result_t test_net_rx_indirect_write(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check INDIRECT_DESC feature */
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    if (cfg->num_queues < 1)
        return TEST_SKIP;

    /* RX on queue 0 */
    uint8_t *rxbuf = vv_alloc_pages(1);

    /* Build indirect descriptor table: one writable entry */
    struct {
        uint64_t addr;
        uint32_t len;
        uint16_t flags;
        uint16_t next;
    } __attribute__((packed)) *indirect = vv_alloc_pages(1);

    indirect[0].addr = vv_virt_to_phys(rxbuf);
    indirect[0].len = 1514;
    indirect[0].flags = VRING_DESC_F_WRITE;
    indirect[0].next = 0;

    /* Post indirect descriptor on queue 0 (RX queue) */
    vv_desc(vr, 0, indirect, 16, VRING_DESC_F_INDIRECT | VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_REQUIRES(N0057, VIRTIO_PCI_DEVICE_NET, test_net_rx_indirect_write,
              "RX buffer with INDIRECT+WRITE flags combined",
              VIRTIO_SPEC_V1_2, "5.1.6.4",
              (1ULL << VIRTIO_F_INDIRECT_DESC), 0);

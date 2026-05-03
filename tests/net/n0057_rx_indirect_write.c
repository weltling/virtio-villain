/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0057: RX buffer with INDIRECT|WRITE flags combined (spec 5.1.6.4)
 *
 * Post an RX buffer that has both INDIRECT and WRITE flags set on
 * the same descriptor. This is a legal combination but tests device
 * handling of indirect writable buffers in the receive path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_rx_indirect_write(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Check INDIRECT_DESC feature */
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << 28))) /* VIRTIO_F_INDIRECT_DESC */
        return TEST_SKIP;

    if (cfg->num_queues < 1)
        return TEST_SKIP;

    /* RX on queue 0 */
    uint8_t *rxbuf = vv_alloc_pages(1);
    memset(rxbuf, 0, PAGE_SIZE);

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
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(indirect), 16,
                       VRING_DESC_F_INDIRECT | VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0057, VIRTIO_PCI_DEVICE_NET, test_net_rx_indirect_write,
              "RX buffer with INDIRECT+WRITE flags combined",
              VIRTIO_SPEC_V1_2, "5.1.6.4");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0351: sparse_q02_msix
 *
 * Spec 4.1.5.1.2: a used buffer notification for a virtqueue goes to
 * the MSI-X vector bound to that virtqueue. Bind queue 0, 1 and 2 to
 * vectors 0, 1 and 2, enable queues 0 and 2 and leave queue 1
 * disabled. MSI-X is enabled with all entries masked, so notifications
 * land in the Pending Bit Array. A read on queue 2 must set PBA bit 2
 * only. A device that indexes its workers by position among the enabled
 * queues treats queue 2 as queue 1 and sets bit 1. B0350 is the dense
 * control.
 */
#include "tests/test.h"
#include "lib/pci.h"

#define MSIX_ENABLE        (1u << 31)
#define MSIX_FUNCTION_MASK (1u << 30)

#define NVEC 3

/* Set up the queues on a fresh device, read on queue 2, check the PBA. */
static test_result_t sparse_q02_read(struct virtio_dev *dev,
                                     volatile uint8_t *pbits)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Setup: fresh init with MQ, PBA clear. */
    virtio_pci_reset(dev);
    if (virtio_pci_init_features(dev, 1ULL << VIRTIO_BLK_F_MQ) < 0)
        TFAIL("init with MQ failed");
    if (cfg->num_queues < 3)
        TSKIP("need three queues");
    /* PBA bit n is set while vector n is pending. */
    if (vv_mmio_read32(pbits) & 7u)
        TSKIP("PBA bits already set: 0x%x", vv_mmio_read32(pbits));

    struct vring q[NVEC];
    for (unsigned i = 0; i < NVEC; i++) {
        vring_alloc(&q[i], 16);
        vring_prepare(dev, &q[i], i);
        /* vring_prepare() suppresses notifications. This test needs them. */
        q[i].avail->flags = 0;
        cfg->queue_msix_vector = i;
        __sync_synchronize();
        if (cfg->queue_msix_vector != i)
            TSKIP("queue %u vector not accepted", i);
        if (i != 1) {
            cfg->queue_enable = 1;
            __sync_synchronize();
        }
    }
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    TSKIP("not implemented");
}

static test_result_t test_blk_sparse_q02_msix(struct virtio_dev *dev,
                                              struct vring *unused)
{
    (void)unused;

    /* Setup: locate the MSI-X table and PBA. */
    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        TFAIL("open PCI config space");
    uint8_t cap = pci_find_capability(fd, PCI_CAP_ID_MSIX);
    if (!cap) {
        close(fd);
        TSKIP("no MSI-X capability");
    }
    uint32_t ctl = pci_cfg_read32(fd, cap);
    uint32_t tbl = pci_cfg_read32(fd, cap + 4);
    uint32_t pba = pci_cfg_read32(fd, cap + 8);
    /* Message Control bits 10:0 hold the table size minus 1. */
    if (((ctl >> 16) & 0x7ff) + 1 < NVEC) {
        close(fd);
        TSKIP("need %u MSI-X vectors", NVEC);
    }
    /* Bits 2:0 select the BAR (BIR), the rest is the offset into it. */
    volatile uint8_t *tbar = pci_map_bar(dev->slot, tbl & 7);
    volatile uint8_t *pbar = pci_map_bar(dev->slot, pba & 7);
    if (!tbar || !pbar) {
        close(fd);
        TFAIL("map MSI-X BAR");
    }
    volatile uint8_t *table = tbar + (tbl & ~7u);
    volatile uint8_t *pbits = pbar + (pba & ~7u);

    /*
     * Setup: mask the entries, then enable MSI-X. A device signals MSI-X
     * vectors, and so sets PBA bits, only while MSI-X is enabled. Bit 0
     * of the Vector Control dword (offset 12) masks the entry.
     */
    uint32_t masks[NVEC];
    for (unsigned v = 0; v < NVEC; v++) {
        volatile uint8_t *vc = table + v * 16 + 12;
        masks[v] = vv_mmio_read32(vc);
        vv_mmio_write32(vc, masks[v] | 1u);
    }
    __sync_synchronize();
    pci_cfg_write32(fd, cap, (ctl | MSIX_ENABLE) & ~MSIX_FUNCTION_MASK);

    test_result_t r = sparse_q02_read(dev, pbits);

    /* Cleanup: reset the device, disable MSI-X and restore the masks. */
    virtio_pci_reset(dev);
    pci_cfg_write32(fd, cap, ctl & ~MSIX_ENABLE);
    __sync_synchronize();
    if (pci_cfg_read32(fd, cap) & MSIX_ENABLE) {
        close(fd);
        TFAIL("MSI-X remained enabled after the test");
    }
    for (unsigned v = 0; v < NVEC; v++)
        vv_mmio_write32(table + v * 16 + 12, masks[v]);
    __sync_synchronize();
    close(fd);
    if (dev->common->device_status != 0)
        TWEDGED("device reset did not complete after the test");
    return r;
}

REGISTER_TEST_REQUIRES(B0351, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_sparse_q02_msix,
                       "Queues 0 and 2 enabled: queue 2 signals its own vector",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_MQ), 3);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0351: sparse_q02_msix
 *
 * Spec 4.1.5.1.2: a used buffer notification for a virtqueue goes to
 * the MSI-X vector bound to that virtqueue. Bind queue 0, 1 and 2 to
 * vectors 0, 1 and 2, enable queues 0 and 2 and leave queue 1
 * disabled. All entries stay masked so notifications land in the
 * Pending Bit Array. A read on queue 2 must set PBA bit 2 only. A
 * device that indexes its workers by position among the enabled queues
 * treats queue 2 as queue 1 and sets bit 1. B0350 is the dense control.
 */
#include "tests/test.h"
#include "lib/pci.h"

#define NVEC 3

static test_result_t test_blk_sparse_q02_msix(struct virtio_dev *dev,
                                              struct vring *unused)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

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
    close(fd);
    /* Message Control bits 10:0 hold the table size minus 1. */
    if (((ctl >> 16) & 0x7ff) + 1 < NVEC)
        TSKIP("need %u MSI-X vectors", NVEC);
    /* Bits 2:0 select the BAR (BIR), the rest is the offset into it. */
    volatile uint8_t *tbar = pci_map_bar(dev->slot, tbl & 7);
    volatile uint8_t *pbar = pci_map_bar(dev->slot, pba & 7);
    if (!tbar || !pbar)
        TFAIL("map MSI-X BAR");
    volatile uint8_t *table = tbar + (tbl & ~7u);
    volatile uint8_t *pbits = pbar + (pba & ~7u);

    /* Setup: fresh init with MQ, entries masked, PBA clear. */
    virtio_pci_reset(dev);
    if (virtio_pci_init_features(dev, 1ULL << VIRTIO_BLK_F_MQ) < 0)
        TFAIL("init with MQ failed");
    if (cfg->num_queues < 3)
        TSKIP("need three queues");
    /* Bit 0 of the Vector Control dword (offset 12) masks the entry. */
    for (unsigned v = 0; v < NVEC; v++) {
        volatile uint8_t *vc = table + v * 16 + 12;
        vv_mmio_write32(vc, vv_mmio_read32(vc) | 1u);
    }
    __sync_synchronize();
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

    /* One read on queue 2, then look at the PBA. */
    struct vring *vr = &q[2];
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    /* Not a valid status, so a byte the device never wrote stays visible. */
    *st = 0xff;
    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, VIRTIO_BLK_SECTOR_SIZE,
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, st, 1, VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, vr, 0));
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("read status 0x%02x", *st);
    usleep(50000);
    uint32_t bits = vv_mmio_read32(pbits) & 7u;
    printf("vv-info PBA after queue 2 completion: 0x%x\n", bits);
    if (bits != 4u)
        TFAIL("queue 2 completion set PBA 0x%x, expected 0x4 (vector 2)",
              bits);
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0351, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_sparse_q02_msix,
                       "Queues 0 and 2 enabled: queue 2 signals its own vector",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_MQ), 3);

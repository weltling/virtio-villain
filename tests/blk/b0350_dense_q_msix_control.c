/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0350: dense_q_msix_control
 *
 * Control for B0345, B0346 and B0351. Same Pending Bit Array based
 * check, but queue 0 is enabled too (vector 0) so the enabled queues
 * are dense. A read on queue 1 must set PBA bit 1 and not bit 0. This
 * passing while B0345 fails isolates the bug to sparse queue
 * activation.
 */
#include "tests/test.h"
#include "lib/pci.h"

#define Q0_VECTOR 0

static test_result_t sparse_q_pba(struct virtio_dev *dev, uint16_t q0_vector,
                                  uint32_t *pba_out)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

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
    if (((ctl >> 16) & 0x7ff) + 1 < 2)
        TSKIP("need two MSI-X vectors");
    /* Bits 2:0 select the BAR (BIR), the rest is the offset into it. */
    volatile uint8_t *tbar = pci_map_bar(dev->slot, tbl & 7);
    volatile uint8_t *pbar = pci_map_bar(dev->slot, pba & 7);
    if (!tbar || !pbar)
        TFAIL("map MSI-X BAR");
    volatile uint8_t *table = tbar + (tbl & ~7u);
    volatile uint8_t *pbits = pbar + (pba & ~7u);

    /* Setup: fresh init with MQ, both entries masked, PBA clear. */
    virtio_pci_reset(dev);
    if (virtio_pci_init_features(dev, 1ULL << VIRTIO_BLK_F_MQ) < 0)
        TFAIL("init with MQ failed");
    if (cfg->num_queues < 2)
        TSKIP("need two queues");
    /* Bit 0 of the Vector Control dword (offset 12) masks the entry. */
    for (unsigned v = 0; v < 2; v++) {
        volatile uint8_t *vc = table + v * 16 + 12;
        vv_mmio_write32(vc, vv_mmio_read32(vc) | 1u);
    }
    __sync_synchronize();
    /* PBA bit n is set while vector n is pending. */
    if (vv_mmio_read32(pbits) & 3u)
        TSKIP("PBA bits already set: 0x%x", vv_mmio_read32(pbits));

    struct vring q0, q1;
    vring_alloc(&q0, 16);
    vring_alloc(&q1, 16);
    vring_prepare(dev, &q0, 0);
    cfg->queue_msix_vector = q0_vector;
    __sync_synchronize();
    if (cfg->queue_msix_vector != q0_vector)
        TSKIP("queue 0 vector not accepted");
    cfg->queue_enable = 1;
    __sync_synchronize();
    vring_prepare(dev, &q1, 1);
    /* vring_prepare() suppresses notifications. This test needs them. */
    q1.avail->flags = 0;
    cfg->queue_msix_vector = 1;
    __sync_synchronize();
    if (cfg->queue_msix_vector != 1)
        TSKIP("queue 1 vector not accepted");
    cfg->queue_enable = 1;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* One read on queue 1, then look at the PBA. */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    /* Not a valid status, so a byte the device never wrote stays visible. */
    *st = 0xff;
    vv_desc(&q1, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(&q1, 1, data, VIRTIO_BLK_SECTOR_SIZE,
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(&q1, 2, st, 1, VRING_DESC_F_WRITE, 0);
    VV_TRY(vv_submit_and_wait(dev, &q1, 0));
    if (*st != VIRTIO_BLK_S_OK)
        TFAIL("read status 0x%02x", *st);
    /*
     * Wait for the first pending bit, then a little longer so a bit for
     * the wrong vector is not missed.
     */
    for (int i = 0; i < 200 && !(vv_mmio_read32(pbits) & 3u); i++)
        usleep(1000);
    usleep(50000);
    *pba_out = vv_mmio_read32(pbits) & 3u;
    printf("vv-info PBA after queue 1 completion: 0x%x\n", *pba_out);
    return TEST_PASS;
}

static test_result_t test_blk_dense_q_msix_control(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    uint32_t pba;

    (void)vr;
    VV_TRY(sparse_q_pba(dev, Q0_VECTOR, &pba));
    if (pba & 1u)
        TFAIL("queue 1 completion signalled queue 0's vector 0 (PBA 0x%x)",
              pba);
    if (!(pba & 2u))
        TFAIL("queue 1 completion did not signal vector 1 (PBA 0x%x)", pba);
    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(B0350, VIRTIO_PCI_DEVICE_BLK,
                       test_blk_dense_q_msix_control,
                       "Dense queues: queue 1 signals MSI-X vector 1 (control)",
                       VIRTIO_SPEC_V1_2, "4.1.5.1.2",
                       VV_FEATURE_BIT(VIRTIO_BLK_F_MQ), 2);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0074: desc_access_platform_no_iommu
 *
 * Packed twin of T0079. Reset the device and renegotiate VERSION_1,
 * RING_PACKED, and ACCESS_PLATFORM without configuring any IOMMU
 * translation, then submit a packed request. With ACCESS_PLATFORM the
 * addresses require platform translation, and none is provided, so the
 * device must reject the I/O rather than treat the raw address as a
 * guest physical address.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_access_platform_no_iommu(struct virtio_dev *dev,
                                                         struct vring_packed *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    virtio_pci_reset(dev);

    cfg->device_feature_select = 1; /* bits 32..63 */
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    uint32_t need = (1U << (VIRTIO_F_VERSION_1 % 32)) |
                    (1U << (VIRTIO_F_ACCESS_PLATFORM % 32)) |
                    (1U << (VIRTIO_F_RING_PACKED % 32));
    if ((offered & need) != need)
        return TEST_SKIP; /* platform flag or packed ring not offered */

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 1;
    __sync_synchronize();
    cfg->driver_feature = need;
    __sync_synchronize();
    cfg->driver_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature = 0;
    __sync_synchronize();

    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(10000);
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("device rejected ACCESS_PLATFORM feature negotiation");

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct vring_packed pvr;
    vring_packed_alloc(&pvr, 16);
    vring_packed_attach(dev, &pvr, 0);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint16_t a0 = pvr.next_avail;
    uint8_t w0 = pvr.wrap_counter;

    pk_append(&pvr, vv_virt_to_phys(hdr), sizeof(*hdr), a0,
              VRING_PACKED_DESC_F_NEXT);
    pk_append(&pvr, vv_virt_to_phys(data), 512, a0,
              VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    pk_append(&pvr, vv_virt_to_phys(status), 1, a0,
              VRING_PACKED_DESC_F_WRITE);

    /* No IOMMU mapping was set up, so the device should reject this. */
    return vv_kick_and_wait_packed(dev, &pvr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0074, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_access_platform_no_iommu,
                     "ACCESS_PLATFORM negotiated on packed ring, no IOMMU",
                     VIRTIO_SPEC_V1_2, "2.8.6");

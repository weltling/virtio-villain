/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0079: desc_with_access_platform_no_iommu
 *
 * Negotiate VIRTIO_F_ACCESS_PLATFORM (bit 33) without providing any
 * IOMMU mapping. The device must reject I/O since the platform flag
 * means addresses need translation but none is configured.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_desc_access_platform_no_iommu(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Reset the device */
    virtio_pci_reset(dev);

    /* Check if the device offers ACCESS_PLATFORM */
    cfg->device_feature_select = VIRTIO_F_ACCESS_PLATFORM / 32;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;
    if (!(offered & (1U << (VIRTIO_F_ACCESS_PLATFORM % 32))))
        return TEST_SKIP; /* device doesn't offer this feature */

    /* ACKNOWLEDGE + DRIVER */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Accept ACCESS_PLATFORM */
    cfg->driver_feature_select = VIRTIO_F_ACCESS_PLATFORM / 32;
    __sync_synchronize();
    cfg->driver_feature = (1U << (VIRTIO_F_ACCESS_PLATFORM % 32));
    __sync_synchronize();

    /* Clear lower feature word */
    cfg->driver_feature_select = 0;
    __sync_synchronize();
    cfg->driver_feature = 0;
    __sync_synchronize();

    /* FEATURES_OK */
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(10000);

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK)"); /* device rejected feature negotiation */

    /* DRIVER_OK */
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Set up a queue and submit I/O (no IOMMU mapping provided) */
    struct vring tvr;
    vring_alloc(&tvr, 16);
    vring_attach(dev, &tvr, 0);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&tvr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&tvr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&tvr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&tvr, 0, 0);
    vring_raw_set_avail_idx(&tvr, 1);

    /* Device should reject this since we have no IOMMU translations */
    return vv_kick_expect_reject(dev, &tvr, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(T0079, VIRTIO_PCI_DEVICE_BLK, test_desc_access_platform_no_iommu,
              "ACCESS_PLATFORM negotiated but no IOMMU translation provided",
              VIRTIO_SPEC_V1_2, "2.7.2",
              (1ULL << VIRTIO_F_ACCESS_PLATFORM), 0);

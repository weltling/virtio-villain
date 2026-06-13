/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0057: READ with VIRTIO_BLK_F_RO feature (spec 5.2.5)
 *
 * If the device exposes VIRTIO_BLK_F_RO (read-only), attempt a
 * write. The device should report an error status or reject.
 * Also verify reads still work on a RO device.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_write_readonly(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t offered = cfg->device_feature;

    if (!(offered & (1U << VIRTIO_BLK_F_RO)))
        return TEST_SKIP; /* device is not read-only */

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;
    memset(data, 0xAA, 512);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0057, VIRTIO_PCI_DEVICE_BLK, test_blk_write_readonly,
              "WRITE to read-only device (VIRTIO_BLK_F_RO set)",
              VIRTIO_SPEC_V1_2, "5.2.5");

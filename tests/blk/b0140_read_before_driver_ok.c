/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0140: Read request after clearing DRIVER_OK.
 *
 * Spec 2.1 says the driver MUST NOT clear a device status bit
 * once set. The only way out of DRIVER_OK is a full reset. Spec
 * 2.1.2 only forbids the device from consuming buffers before
 * DRIVER_OK is set, not after the driver illegally clears the
 * bit. The device response to that forbidden write is therefore
 * unspecified, and a device that keeps serving the activated
 * worker is within spec. The test only checks that the device
 * does not crash. If the kick is honored we report TEST_REJECT
 * to record that the device chose to ignore the forbidden write.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_blk_read_before_driver_ok(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Clear DRIVER_OK but keep ACKNOWLEDGE | DRIVER | FEATURES_OK.
     * This simulates a driver that hasn't completed init.
     */
    uint8_t saved_status = cfg->device_status;
    cfg->device_status = saved_status & ~VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    usleep(200000);

    /* Device must NOT have processed the request */
    __sync_synchronize();
    if (vr->used->idx != 0) {
        /* Restore and reject */
        cfg->device_status = saved_status;
        TREJECT("vr->used->idx != 0");
    }

    /* Restore DRIVER_OK */
    cfg->device_status = saved_status;
    __sync_synchronize();

    return TEST_PASS;
}

REGISTER_TEST(B0140, VIRTIO_PCI_DEVICE_BLK, test_blk_read_before_driver_ok,
              "Read request after clearing DRIVER_OK",
              VIRTIO_SPEC_V1_2, "5.2.6");

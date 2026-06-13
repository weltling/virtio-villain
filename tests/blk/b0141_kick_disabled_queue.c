/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0141: Kick after queue_enable=0.
 *
 * Spec 4.1.4.3.2 says the driver MUST NOT write 0 to queue_enable
 * after DRIVER_OK. The device response to such a forbidden write is
 * unspecified, so a device that still services the kick is within
 * spec. The test only checks that the device does not crash and
 * remains usable. If the kick is honored we report TEST_REJECT to
 * record that the device chose to ignore the forbidden write.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_kick_disabled_queue(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

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

    /* Disable the queue */
    cfg->queue_select = vr->queue;
    __sync_synchronize();
    cfg->queue_enable = 0;
    __sync_synchronize();

    /* Kick the now-disabled queue */
    virtio_pci_kick(dev, vr->queue);
    usleep(200000);

    __sync_synchronize();
    if (vr->used->idx != 0) {
        /* Re-enable before returning */
        cfg->queue_enable = 1;
        TREJECT("vr->used->idx != 0");
    }

    /* Re-enable the queue */
    cfg->queue_enable = 1;
    __sync_synchronize();

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(B0141, VIRTIO_PCI_DEVICE_BLK, test_blk_kick_disabled_queue,
              "Kick after queue_enable=0",
              VIRTIO_SPEC_V1_2, "5.2.6");

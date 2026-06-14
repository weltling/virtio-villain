/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0095: queue toggle off then on between requests
 *
 * Spec 4.1.4.3.2 lets the driver clear queue_enable to take a
 * queue out of service. The spec does not require the queue to
 * be reset when re enabled with the same addresses, so flipping
 * enable from 1 to 0 to 1 should leave the queue functional. A
 * second request after the toggle must complete normally.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t do_one_read(struct virtio_dev *dev, struct vring *vr,
                                 uint16_t base_slot, uint16_t avail_slot,
                                 uint64_t sector)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = sector;
    *st = 0xFF;

    vring_raw_set_desc(vr, base_slot, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, base_slot + 1);
    vring_raw_set_desc(vr, base_slot + 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base_slot + 2);
    vring_raw_set_desc(vr, base_slot + 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_slot, base_slot);
    vring_raw_set_avail_idx(vr, avail_slot + 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

static test_result_t test_queue_toggle(struct virtio_dev *dev,
                                       struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    test_result_t r = do_one_read(dev, vr, 0, 0, 0);
    if (r != TEST_PASS)
        return r;

    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(5000);
    cfg->queue_enable = 1;
    __sync_synchronize();

    return do_one_read(dev, vr, 3, 1, 1);
}

REGISTER_TEST(T0095, VIRTIO_PCI_DEVICE_BLK, test_queue_toggle,
              "queue_enable toggle off then on still serves requests",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");

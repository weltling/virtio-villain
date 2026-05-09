/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0203: virtio-blk snapshot does not break the device.
 *
 * Spec 1.0 host requirement on snapshot is that the device state is
 * captured in a way that lets it resume serving requests. The
 * sidecar takes a snapshot of the running VM mid test. The guest
 * runs a read on sector 0 before the snapshot window and another
 * read on sector 1 after the snapshot window. Both must complete
 * with status OK.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t do_read(struct virtio_dev *dev, struct vring *vr,
                             uint64_t sector)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = sector;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    uint16_t idx = vr->avail->idx;
    vring_raw_set_avail(vr, idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, idx + 1);
    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS * 4);
    if (r != TEST_PASS)
        return r;
    return *st == 0 ? TEST_PASS : TEST_FAIL;
}

static test_result_t test_blk_snapshot_during_io(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    test_result_t r = do_read(dev, vr, 0);
    if (r != TEST_PASS)
        return r;
    sleep(2);
    return do_read(dev, vr, 1);
}

REGISTER_TEST(B0203, VIRTIO_PCI_DEVICE_BLK, test_blk_snapshot_during_io,
              "virtio-blk survives an in place snapshot mid I/O",
              VIRTIO_SPEC_V1_2, "2.6");

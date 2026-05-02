/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0011: packed_wrap_counter_confusion
 *
 * Submit descriptors with the wrong wrap counter phase. Set AVAIL bit
 * to the opposite of what the device expects, then flip it. This
 * creates a race where the device may see a stale or toggled
 * descriptor.
 * Spec 2.8.21: wrap counter determines valid AVAIL/USED flag meaning.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_packed_wrap_counter_confusion(
    struct virtio_dev *dev, struct vring_packed *vr)
{
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

    /*
     * Set descriptors with WRONG wrap counter polarity.
     * If wrap_counter is 1 (initial), the correct AVAIL bit is 1 and
     * USED bit is 0. We do the opposite: AVAIL=0, USED=1.
     * This makes the descriptor look "already used" to the device.
     */
    uint16_t wrong_avail = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_AVAIL;
    uint16_t wrong_used = vr->wrap_counter ? VRING_PACKED_DESC_F_USED : 0;

    vring_packed_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), 0,
                              VRING_PACKED_DESC_F_NEXT |
                              wrong_avail | wrong_used);
    vring_packed_raw_set_desc(vr, 1, data_phys, 512, 1,
                              VRING_PACKED_DESC_F_NEXT |
                              VRING_PACKED_DESC_F_WRITE |
                              wrong_avail | wrong_used);
    vring_packed_raw_set_desc(vr, 2, status_phys, 1, 2,
                              VRING_PACKED_DESC_F_WRITE |
                              wrong_avail | wrong_used);
    __sync_synchronize();

    /* Kick - device should see descriptors as NOT available */
    virtio_pci_kick(dev, 0);
    usleep(100000);

    /*
     * Now flip to correct polarity (simulating wrap counter confusion).
     * The device may have already scanned and rejected, or may now
     * pick them up with stale data.
     */
    uint16_t correct_avail = vr->wrap_counter ? VRING_PACKED_DESC_F_AVAIL : 0;
    uint16_t correct_used = vr->wrap_counter ? 0 : VRING_PACKED_DESC_F_USED;

    vr->desc[0].flags = VRING_PACKED_DESC_F_NEXT | correct_avail | correct_used;
    __sync_synchronize();
    vr->desc[1].flags = VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE |
                        correct_avail | correct_used;
    __sync_synchronize();
    vr->desc[2].flags = VRING_PACKED_DESC_F_WRITE | correct_avail | correct_used;
    __sync_synchronize();

    virtio_pci_kick(dev, 0);
    usleep(500000);

    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0011, VIRTIO_PCI_DEVICE_BLK, test_packed_wrap_counter_confusion,
                     "Wrap counter confusion / toggle race",
                     VIRTIO_SPEC_V1_2, "2.8.21");

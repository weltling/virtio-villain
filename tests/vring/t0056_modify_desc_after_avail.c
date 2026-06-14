/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0056: modify_desc_after_avail
 *
 * Place a valid descriptor in the available ring, kick, then
 * immediately rewrite the descriptor to point to a different address
 * and change its length. Tests whether the device reads the descriptor
 * atomically or may see a partial update.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_modify_desc_after_avail(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);
    uint8_t *bogus = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);
    uint64_t bogus_phys = vv_virt_to_phys(bogus);

    /* Set up valid chain */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    __sync_synchronize();

    virtio_pci_kick(dev, 0);

    /* Race: immediately rewrite desc[1] to point elsewhere */
    vring_raw_set_desc(vr, 1, bogus_phys, 4096,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    __sync_synchronize();

    usleep(500000);

    /* Survival = pass */
    return TEST_PASS;
}

REGISTER_TEST(T0056, VIRTIO_PCI_DEVICE_BLK, test_modify_desc_after_avail,
              "Modify descriptor after making available",
              VIRTIO_SPEC_V1_2, "2.7.5");

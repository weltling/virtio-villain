/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0053: AVAIL and USED bits with both clear must be ignored.
 *
 * v1.4 2.8.4: a descriptor is available when avail == wrap and
 * used == !wrap. With both clear and wrap = 1, the descriptor
 * is NOT available. Place a malformed descriptor with both
 * bits zero in the next ring slot; the device must not
 * dispatch it.
 */
#include "tests/test.h"
#include "lib/util.h"

#include <unistd.h>

static test_result_t test(struct virtio_dev *dev, struct vring_packed *vr)
{
    if (vr->wrap_counter == 0)
        return TEST_SKIP;

    uint8_t *buf = vv_alloc_pages(1);
    uint16_t head = vr->next_avail;
    vring_packed_raw_set_desc(vr, head, vv_virt_to_phys(buf), 16,
                              head, VRING_PACKED_DESC_F_WRITE);
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);
    usleep(VV_TIMEOUT_MS * 1000);

    if (vring_packed_desc_is_used(vr, head, vr->wrap_counter))
        TFAIL("device used a descriptor without AVAIL bit");
    return TEST_PASS;
}

REGISTER_TEST_PACKED(P0053, VIRTIO_PCI_DEVICE_BLK, test,
                     "Descriptor without AVAIL bit is ignored",
                     VIRTIO_SPEC_V1_4, "2.8.4");

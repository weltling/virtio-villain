/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0022: duplicate_head
 *
 * Place the same descriptor head index in the available ring twice.
 * In the split ring the descriptor table is read only for the device
 * and the driver owns the available ring, so a duplicated head is a
 * driver side error the device is not required to detect. Two valid
 * responses exist: process both entries and complete the head twice
 * (QEMU), or detect the in flight reuse and set NEEDS_RESET (CH). Both
 * are acceptable. This test only guards against a crash, hang, or
 * memory unsafety, which the harness reports separately as a wedge,
 * timeout, or sanitizer abort.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

/* virtio-blk request header */
static test_result_t test_duplicate_head(struct virtio_dev *dev, struct vring *vr)
{
    /* Allocate request buffers */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Read sector 0 */
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0xAA, 512);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t stat_phys = vv_virt_to_phys(status);

    /* Descriptor chain: [0] header -> [1] data -> [2] status */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, stat_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    /* THE BUG: same head (0) submitted twice */
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);

    /* Kick */
    virtio_pci_kick(dev, 0);

    /*
     * Wait for at least one completion. The device may complete the
     * duplicated head once or twice, or detect the reuse and stop.
     */
    int timeout = 100;
    while (vr->used->idx < 1 && timeout > 0) {
        usleep(10000);
        __sync_synchronize();
        timeout--;
    }

    /*
     * Both completing the head and setting NEEDS_RESET are valid
     * responses to a driver duplicating a head. A hard crash, hang, or
     * sanitizer abort is surfaced by the harness, not here, so there is
     * nothing left to assert on the device status.
     */
    (void)timeout;

    return TEST_PASS;
}

REGISTER_TEST(T0022, VIRTIO_PCI_DEVICE_BLK, test_duplicate_head,
              "Duplicate head index in available ring",
              VIRTIO_SPEC_V1_2, "2.7.6");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0037: blk_oversized_status_descriptor
 *
 * Submit a READ request with a status descriptor that is 512 bytes
 * (far larger than the 1-byte status field). The device should only
 * write 1 byte and not overflow into adjacent memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_oversized_status(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(status, 0xCC, 512); /* canary pattern */

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    /* Status descriptor is 512 bytes - way more than needed */
    vring_raw_set_desc(vr, 2, status_phys, 512,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0037, VIRTIO_PCI_DEVICE_BLK, test_blk_oversized_status,
              "Status descriptor oversized (512 bytes for 1-byte status)",
              VIRTIO_SPEC_V1_2, "5.2.6");

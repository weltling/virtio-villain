/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0009: desc_write_flag_wrong_read
 *
 * Submit a virtio-blk read request where the data buffer descriptor is
 * marked as device-readable (missing VRING_DESC_F_WRITE). The spec says
 * the device writes data into writable descriptors; a readable data
 * buffer means the device has nowhere to place the read result.
 *
 * A VMM that ignores the flag may write into guest memory that the
 * driver intended as read-only, or it may crash trying to validate
 * the request layout.
 *
 * Correct behavior: reject the request (put an error status in the
 * used ring) or at minimum not corrupt memory.
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

static test_result_t test_desc_write_flag_wrong(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0xAA, 512);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Descriptor chain:
     *   [0] header (readable) - correct
     *   [1] data buffer (readable!) - WRONG, should be writable for IN
     *   [2] status byte (writable) - correct
     */
    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT, 2); /* missing WRITE flag */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0009, VIRTIO_PCI_DEVICE_BLK, test_desc_write_flag_wrong,
              "Data descriptor missing WRITE flag on read",
              VIRTIO_SPEC_V1_2, "2.7.5");

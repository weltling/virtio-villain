/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0010: blk_write_to_ro_device
 *
 * Submit a write (OUT) request. Since the test disk is a temporary file
 * without RO flag, this tests that writes are handled. If the device
 * advertises RO, a write should get IOERR.
 *
 * Note: This test always exercises the write path. If the VMM
 * doesn't advertise RO, the write succeeds (PASS). If it does
 * advertise RO, it should reject with IOERR (also PASS).
 * It only FAILs if the VMM hangs or crashes.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_write(struct virtio_dev *dev,
                                    struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0x55, 512);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT, 2); /* readable for OUT */
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0010, VIRTIO_PCI_DEVICE_BLK, test_blk_write,
              "Write (OUT) request to device",
              VIRTIO_SPEC_V1_2, "5.2.6");

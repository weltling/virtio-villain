/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0161: blk_get_id_with_readonly_buffer
 *
 * Submit GET_ID but make the data buffer device readable instead
 * of device writable. Spec 5.2.6.1 says the data buffer for
 * GET_ID must be writable by the device. The device must reject
 * the malformed request.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_getid_readonly(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_ID;
    hdr->ioprio = 0;
    hdr->sector = 0;
    memset(data, 0, 20);
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    /* Data buffer: NOT writable (missing VRING_DESC_F_WRITE) */
    vring_raw_set_desc(vr, 1, data_phys, 20,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0161, VIRTIO_PCI_DEVICE_BLK, test_blk_getid_readonly,
              "GET_ID with device readable data buffer",
              VIRTIO_SPEC_V1_2, "5.2.6.1");

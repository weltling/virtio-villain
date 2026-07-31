/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0221: extra_writable_on_no_data
 *
 * Attach a writable buffer to a TEST UNIT READY that transfers no data.
 * The device tolerates the extra buffer, completing with a defined
 * response, and the host stays alive.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_extra_writable(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *extra = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x00;      /* TEST UNIT READY */
    resp->response = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(extra), 16,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK &&
        resp->response != VIRTIO_SCSI_S_OVERRUN)
        TFAIL("response 0x%02x", resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0221, VIRTIO_PCI_DEVICE_SCSI, test_scsi_extra_writable,
                "Extra writable buffer on a no-data command is handled",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

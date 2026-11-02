/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0003: inquiry
 *
 * Issue a standard SCSI INQUIRY on the request queue and confirm the
 * device returns a good status with a peripheral device type of a
 * direct access block device. INQUIRY is answered even while a UNIT
 * ATTENTION is pending, so a single command suffices.
 */
#include "lib/virtio_test.h"

static test_result_t test_scsi_inquiry(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 36;        /* allocation length */

    resp->response = 0xFF;
    resp->status = 0xFF;
    data[0] = 0xFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            2);
    vv_desc(vr, 2, data, 36, VRING_DESC_F_WRITE, 0);

    VV_TRY(vv_submit_and_wait(dev, vr, 0));

    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x, expected OK", resp->response);
    if (resp->status != 0)
        TFAIL("scsi status 0x%02x, expected 0", resp->status);
    /* Peripheral device type in the low five bits, 0 = direct access. */
    if ((data[0] & 0x1f) != 0x00)
        TFAIL("peripheral device type 0x%02x, expected 0", data[0] & 0x1f);

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0003, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry,
                "INQUIRY reports a direct access block device",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

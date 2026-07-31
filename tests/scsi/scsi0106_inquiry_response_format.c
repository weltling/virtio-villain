/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0106: inquiry_response_format
 *
 * The standard INQUIRY response data format field reports the current
 * standard value of 2.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inq_format(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 96;
    data[3] = 0xFF;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 96,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if ((data[3] & 0x0f) != 2)
        TFAIL("response data format 0x%02x, expected 2", data[3] & 0x0f);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0106, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inq_format,
                "INQUIRY reports response data format 2",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0103: inquiry_peripheral_byte
 *
 * The standard INQUIRY data begins with a peripheral qualifier and
 * device type byte. For the attached direct access block device with a
 * connected logical unit both are zero, so the byte is 0x00.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inq_byte0(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 96;
    data[0] = 0xFF;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 96,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (data[0] != 0x00)
        TFAIL("peripheral byte 0x%02x, expected 0x00", data[0]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0103, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inq_byte0,
                "INQUIRY peripheral qualifier and type are zero",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0228: inquiry_lun1
 *
 * INQUIRY addressed to LUN 1 returns a direct access block device,
 * confirming the second logical unit is reachable.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_lun1(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 36;
    data[0] = 0xFF;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 36,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if ((data[0] & 0x1f) != 0x00)
        TFAIL("peripheral device type 0x%02x, expected 0", data[0] & 0x1f);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0228, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_lun1,
                "INQUIRY to LUN 1 reports a direct access device",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

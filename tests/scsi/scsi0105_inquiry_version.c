/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0105: inquiry_version
 *
 * The standard INQUIRY version byte reports a ratified SCSI standard,
 * at least SPC-2, so the value is 3 or greater.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inq_version(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 96;
    data[2] = 0x00;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 96,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (data[2] < 3)
        TFAIL("version 0x%02x, expected 3 or more", data[2]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0105, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inq_version,
                "INQUIRY reports a ratified SCSI version",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

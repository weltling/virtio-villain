/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0022: report_luns
 *
 * REPORT LUNS returns a good status and a LUN list whose length field
 * accounts for at least the single attached logical unit.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_report_luns(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xA0;      /* REPORT LUNS */
    req->cdb[8] = 1;         /* allocation length 256 bytes, matches buffer */
    req->cdb[9] = 0;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 256,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    uint32_t list_len = scsi_be32(data);
    if (list_len < 8)
        TFAIL("LUN list length %u, expected at least 8", list_len);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0022, VIRTIO_PCI_DEVICE_SCSI, test_scsi_report_luns,
                "REPORT LUNS lists the attached logical unit",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

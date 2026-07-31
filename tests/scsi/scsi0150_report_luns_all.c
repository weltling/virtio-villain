/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0150: report_luns_all
 *
 * REPORT LUNS with select report 0x02 for all logical units returns a
 * good status and lists the attached logical unit.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_report_luns_all(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xA0;      /* REPORT LUNS */
    req->cdb[2] = 0x02;      /* all logical units */
    req->cdb[8] = 1;         /* allocation length 256 */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 256,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (scsi_be32(data) < 8)
        TFAIL("LUN list length %u", scsi_be32(data));
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0150, VIRTIO_PCI_DEVICE_SCSI, test_scsi_report_luns_all,
                "REPORT LUNS select report all lists the logical unit",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

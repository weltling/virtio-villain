/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0071: inquiry_zero_alloc
 *
 * INQUIRY with an allocation length of zero moves no data and returns a
 * good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_zero(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 0;         /* allocation length zero */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, NULL, 0,
                                  SCSI_DATA_NONE, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0071, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_zero,
                "INQUIRY with zero allocation length returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

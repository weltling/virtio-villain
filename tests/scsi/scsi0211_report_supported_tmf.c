/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0211: report_supported_tmf
 *
 * MAINTENANCE IN report supported task management functions returns a
 * defined status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_report_tmf(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xA3;      /* MAINTENANCE IN */
    req->cdb[1] = 0x0D;      /* report supported task management functions */
    req->cdb[9] = 32;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 32,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (!scsi_status_defined(resp->status))
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0211, VIRTIO_PCI_DEVICE_SCSI, test_scsi_report_tmf,
                "Report supported task management functions returns a status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

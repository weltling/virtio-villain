/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0185: inquiry_vpd_vendor
 *
 * INQUIRY for a vendor specific VPD page returns a defined status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_vpd_c0(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0xC0;      /* vendor specific page */
    req->cdb[4] = 64;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 64,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (!scsi_status_defined(resp->status))
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0185, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_vpd_c0,
                "INQUIRY for a vendor VPD page returns a defined status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

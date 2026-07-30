/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0024: inquiry_vpd_83
 *
 * INQUIRY with the EVPD bit for page 0x83 returns the device
 * identification page, whose page code field echoes 0x83.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_vpd83(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0x83;      /* device identification */
    req->cdb[4] = 128;       /* allocation length */
    data[1] = 0xFF;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 128,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (data[1] != 0x83)
        TFAIL("page code 0x%02x, expected 0x83", data[1]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0024, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_vpd83,
                "INQUIRY VPD page 0x83 returns device identification",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

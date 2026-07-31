/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0065: inquiry_vpd_b2
 *
 * INQUIRY for VPD page 0xB2 returns the logical block provisioning
 * page, whose page code field echoes 0xB2.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_vpdb2(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0xB2;      /* logical block provisioning */
    req->cdb[4] = 64;
    data[1] = 0xFF;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 64,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (data[1] != 0xB2)
        TFAIL("page code 0x%02x, expected 0xB2", data[1]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0065, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_vpdb2,
                "INQUIRY VPD page 0xB2 returns logical block provisioning",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

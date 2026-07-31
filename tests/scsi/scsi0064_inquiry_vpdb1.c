/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0064: inquiry_vpd_b1
 *
 * INQUIRY for VPD page 0xB1 returns the block device characteristics
 * page, whose page code field echoes 0xB1.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inquiry_vpdb1(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0xB1;      /* block device characteristics */
    req->cdb[4] = 64;
    data[1] = 0xFF;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 64,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (data[1] != 0xB1)
        TFAIL("page code 0x%02x, expected 0xB1", data[1]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0064, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inquiry_vpdb1,
                "INQUIRY VPD page 0xB1 returns block device characteristics",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

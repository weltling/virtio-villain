/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0113: vpdb0_page_length
 *
 * The block limits VPD page 0xB0 reports a nonzero page length so it
 * carries the block limit fields.
 */
#include "lib/scsi_util.h"

static test_result_t test_scsi_vpdb0_len(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0xB0;      /* block limits */
    req->cdb[4] = 64;

    VV_TRY(scsi_do_cmd(dev, vr, req, resp, data, 64, SCSI_DATA_IN, 0));
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    unsigned page_len = ((unsigned)data[2] << 8) | data[3];
    if (page_len == 0)
        TFAIL("page length is zero");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0113, VIRTIO_PCI_DEVICE_SCSI, test_scsi_vpdb0_len,
                "Block limits page carries its fields",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

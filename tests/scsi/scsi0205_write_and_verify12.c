/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0205: write_and_verify12
 *
 * WRITE AND VERIFY(12) of one block returns a good status once the
 * power-on UNIT ATTENTION is cleared.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_wv12(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++)
        data[i] = (uint8_t)(i + 19);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xAE;      /* WRITE AND VERIFY(12) */
    req->cdb[5] = 65;
    req->cdb[9] = 1;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 512,
                                  SCSI_DATA_OUT, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0205, VIRTIO_PCI_DEVICE_SCSI, test_scsi_wv12,
                "WRITE AND VERIFY(12) returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

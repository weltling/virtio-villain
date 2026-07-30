/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0021: request_sense
 *
 * REQUEST SENSE returns a good status and a sense buffer whose first
 * byte carries a valid fixed or descriptor format response code.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_request_sense(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x03;      /* REQUEST SENSE */
    req->cdb[4] = 18;        /* allocation length */
    data[0] = 0x00;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 18,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    /* Fixed (0x70/0x71) or descriptor (0x72/0x73) format response code. */
    if ((data[0] & 0x70) != 0x70)
        TFAIL("sense response code 0x%02x", data[0]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0021, VIRTIO_PCI_DEVICE_SCSI, test_scsi_request_sense,
                "REQUEST SENSE returns a valid sense response code",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

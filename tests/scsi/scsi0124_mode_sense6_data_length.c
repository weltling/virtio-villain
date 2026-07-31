/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0124: mode_sense6_data_length
 *
 * The MODE SENSE(6) parameter header reports a nonzero mode data length
 * covering the returned pages.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_ms6_len(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x1A;      /* MODE SENSE(6) */
    req->cdb[2] = 0x3F;      /* all pages */
    req->cdb[4] = 192;
    data[0] = 0x00;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 192,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (data[0] == 0)
        TFAIL("mode data length is zero");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0124, VIRTIO_PCI_DEVICE_SCSI, test_scsi_ms6_len,
                "MODE SENSE(6) reports a nonzero mode data length",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

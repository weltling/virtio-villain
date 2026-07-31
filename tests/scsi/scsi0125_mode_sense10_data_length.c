/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0125: mode_sense10_data_length
 *
 * The MODE SENSE(10) parameter header reports a nonzero mode data
 * length covering the returned pages.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_ms10_len(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x5A;      /* MODE SENSE(10) */
    req->cdb[2] = 0x3F;      /* all pages */
    req->cdb[8] = 192;
    data[0] = 0x00;
    data[1] = 0x00;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 192,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if ((((unsigned)data[0] << 8) | data[1]) == 0)
        TFAIL("mode data length is zero");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0125, VIRTIO_PCI_DEVICE_SCSI, test_scsi_ms10_len,
                "MODE SENSE(10) reports a nonzero mode data length",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

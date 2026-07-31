/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0073: mode_sense_dbd
 *
 * MODE SENSE(6) with the disable block descriptors bit returns a good
 * status once the power-on UNIT ATTENTION is cleared.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_mode_sense_dbd(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x1A;      /* MODE SENSE(6) */
    req->cdb[1] = 0x08;      /* disable block descriptors */
    req->cdb[2] = 0x3F;      /* all pages */
    req->cdb[4] = 192;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 192,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0073, VIRTIO_PCI_DEVICE_SCSI, test_scsi_mode_sense_dbd,
                "MODE SENSE(6) with DBD returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

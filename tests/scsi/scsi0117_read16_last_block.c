/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0117: read16_last_block
 *
 * READ(16) of the final block at LBA 32767 returns a good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read16_last(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x88;      /* READ(16) */
    req->cdb[8] = 0x7F;      /* LBA 32767 in the low bytes */
    req->cdb[9] = 0xFF;
    req->cdb[13] = 1;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 512,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0117, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read16_last,
                "READ(16) of the last block returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0233: read16_lun1_4k
 *
 * READ(16) of one 4096-byte block from LUN 1 returns a good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read16_lun1(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x00;      /* TEST UNIT READY clears UA */
    scsi_do_cmd(dev, vr, req, resp, NULL, 0, SCSI_DATA_NONE, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x88;      /* READ(16) */
    req->cdb[13] = 1;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 4096,
                                  SCSI_DATA_IN, 1);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0233, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read16_lun1,
                "READ(16) of a 4K block from LUN 1 returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

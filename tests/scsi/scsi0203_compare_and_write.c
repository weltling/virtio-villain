/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0203: compare_and_write
 *
 * Zero a block, then COMPARE AND WRITE with a matching compare block
 * and a new write block, and confirm the atomic compare and write
 * succeeds with a good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_caw(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *zero = vv_alloc_pages(1);
    uint8_t *caw = vv_alloc_pages(1);

    memset(zero, 0, 512);
    memset(caw, 0, 1024);            /* compare block stays zero */
    for (int i = 0; i < 512; i++)
        caw[512 + i] = (uint8_t)(i + 3);  /* write block */

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    /* Ensure the target block is zero so the compare matches. */
    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 90;
    req->cdb[8] = 1;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, zero, 512,
                                  SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("prep write status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x89;      /* COMPARE AND WRITE */
    req->cdb[9] = 90;
    req->cdb[13] = 1;
    r = scsi_do_cmd(dev, vr, req, resp, caw, 1024, SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("compare and write status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0203, VIRTIO_PCI_DEVICE_SCSI, test_scsi_caw,
                "COMPARE AND WRITE with a matching block succeeds",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

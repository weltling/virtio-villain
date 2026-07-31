/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0118: write16_read16_high_lba
 *
 * WRITE(16) a pattern near the end of the disk, read it back with
 * READ(16) and confirm the data matches.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_wr16_high(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++)
        wbuf[i] = (uint8_t)(i ^ 0x3C);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    /* LBA 32000. */
    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x8A;      /* WRITE(16) */
    req->cdb[8] = 0x7D;
    req->cdb[9] = 0x00;
    req->cdb[13] = 1;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, wbuf, 512,
                                  SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("write status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x88;      /* READ(16) */
    req->cdb[8] = 0x7D;
    req->cdb[9] = 0x00;
    req->cdb[13] = 1;
    memset(rbuf, 0, 512);
    r = scsi_do_cmd(dev, vr, req, resp, rbuf, 512, SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read status 0x%02x", resp->status);
    if (memcmp(wbuf, rbuf, 512) != 0)
        TFAIL("read data does not match written data");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0118, VIRTIO_PCI_DEVICE_SCSI, test_scsi_wr16_high,
                "WRITE(16) then READ(16) near end of disk round trips",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

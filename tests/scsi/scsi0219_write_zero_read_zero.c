/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0219: write_zero_read_zero
 *
 * Write a block of zeros, read it back and confirm every byte is zero.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_zero_roundtrip(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    memset(wbuf, 0, 512);
    memset(rbuf, 0xAA, 512);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 95;
    req->cdb[8] = 1;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, wbuf, 512,
                                  SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("write status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 95;
    req->cdb[8] = 1;
    r = scsi_do_cmd(dev, vr, req, resp, rbuf, 512, SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read status 0x%02x", resp->status);
    for (int i = 0; i < 512; i++)
        if (rbuf[i] != 0)
            TFAIL("byte %d is 0x%02x, expected zero", i, rbuf[i]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0219, VIRTIO_PCI_DEVICE_SCSI, test_scsi_zero_roundtrip,
                "A written zero block reads back as zeros",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

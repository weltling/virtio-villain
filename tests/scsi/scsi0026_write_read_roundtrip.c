/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0026: write_read_roundtrip
 *
 * WRITE(10) a known pattern to a block, then READ(10) it back and
 * confirm the data matches, exercising the data-out and data-in paths.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_write_read(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++)
        wbuf[i] = (uint8_t)(i ^ 0xA5);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    /* WRITE(10) one block at LBA 100. */
    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 100;       /* LBA */
    req->cdb[8] = 1;         /* transfer length */
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, wbuf, 512,
                                  SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("write status 0x%02x", resp->status);

    /* READ(10) the same block back. */
    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 100;
    req->cdb[8] = 1;
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

REGISTER_TEST_Q(SCSI0026, VIRTIO_PCI_DEVICE_SCSI, test_scsi_write_read,
                "WRITE(10) then READ(10) round trips the data",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

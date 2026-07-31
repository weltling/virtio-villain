/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0146: write_sync_read_durability
 *
 * Write a pattern with force unit access, flush the cache with
 * SYNCHRONIZE CACHE(10), then read the block back and confirm the data
 * survived.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_durability(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++)
        wbuf[i] = (uint8_t)(i + 0x33);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[1] = 0x08;      /* force unit access */
    req->cdb[5] = 210;
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
    req->cdb[0] = 0x35;      /* SYNCHRONIZE CACHE(10) */
    r = scsi_do_cmd(dev, vr, req, resp, NULL, 0, SCSI_DATA_NONE, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("sync status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 210;
    req->cdb[8] = 1;
    memset(rbuf, 0, 512);
    r = scsi_do_cmd(dev, vr, req, resp, rbuf, 512, SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read status 0x%02x", resp->status);
    if (memcmp(wbuf, rbuf, 512) != 0)
        TFAIL("data did not survive the flush");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0146, VIRTIO_PCI_DEVICE_SCSI, test_scsi_durability,
                "Write then flush then read preserves the data",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

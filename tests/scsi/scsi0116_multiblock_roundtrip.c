/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0116: multiblock_roundtrip
 *
 * WRITE(10) eight blocks of a known pattern, read them back with
 * READ(10) and confirm the whole 4 KiB transfer matches.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_multiblock_rt(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    for (int i = 0; i < 4096; i++)
        wbuf[i] = (uint8_t)(i * 7 + 1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 100;
    req->cdb[8] = 8;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, wbuf, 4096,
                                  SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("write status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 100;
    req->cdb[8] = 8;
    memset(rbuf, 0, 4096);
    r = scsi_do_cmd(dev, vr, req, resp, rbuf, 4096, SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read status 0x%02x", resp->status);
    if (memcmp(wbuf, rbuf, 4096) != 0)
        TFAIL("read data does not match written data");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0116, VIRTIO_PCI_DEVICE_SCSI, test_scsi_multiblock_rt,
                "Eight block write then read round trips the data",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

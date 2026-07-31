/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0161: write12_read12_roundtrip
 *
 * WRITE(12) eight blocks of a known pattern, read them back with
 * READ(12) and confirm the whole 4 KiB transfer matches.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_w12_rt(struct virtio_dev *dev,
                                      struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    for (int i = 0; i < 4096; i++)
        wbuf[i] = (uint8_t)(i * 3 + 13);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xAA;      /* WRITE(12) */
    req->cdb[5] = 160;
    req->cdb[9] = 8;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, wbuf, 4096,
                                  SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("write status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xA8;      /* READ(12) */
    req->cdb[5] = 160;
    req->cdb[9] = 8;
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

REGISTER_TEST_Q(SCSI0161, VIRTIO_PCI_DEVICE_SCSI, test_scsi_w12_rt,
                "WRITE(12) then READ(12) of eight blocks round trips",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

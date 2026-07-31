/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0193: write16_16blocks_roundtrip
 *
 * WRITE(16) sixteen blocks of a known pattern, read them back with
 * READ(16) and confirm the whole 8 KiB transfer matches.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_w16_rt(struct virtio_dev *dev,
                                      struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(2);
    uint8_t *rbuf = vv_alloc_pages(2);

    for (int i = 0; i < 8192; i++)
        wbuf[i] = (uint8_t)(i * 11 + 5);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x8A;      /* WRITE(16) */
    req->cdb[8] = 0x02;      /* LBA 600 */
    req->cdb[9] = 0x58;
    req->cdb[13] = 16;
    test_result_t r = scsi_do_cmd_paged(dev, vr, req, resp, wbuf, 8192,
                                        SCSI_DATA_OUT, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("write status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x88;      /* READ(16) */
    req->cdb[8] = 0x02;
    req->cdb[9] = 0x58;
    req->cdb[13] = 16;
    memset(rbuf, 0, 8192);
    r = scsi_do_cmd_paged(dev, vr, req, resp, rbuf, 8192,
                          SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read status 0x%02x", resp->status);
    if (memcmp(wbuf, rbuf, 8192) != 0)
        TFAIL("read data does not match written data");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0193, VIRTIO_PCI_DEVICE_SCSI, test_scsi_w16_rt,
                "Sixteen block WRITE(16) then READ(16) round trips",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

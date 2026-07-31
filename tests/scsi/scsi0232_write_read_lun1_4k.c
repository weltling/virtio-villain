/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0232: write_read_lun1_4k
 *
 * WRITE(10) a 4096-byte block to LUN 1, read it back and confirm the
 * data matches, covering a full 4K logical block round trip.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_wr_lun1(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    for (int i = 0; i < 4096; i++)
        wbuf[i] = (uint8_t)(i * 13 + 7);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x00;      /* TEST UNIT READY clears UA */
    scsi_do_cmd(dev, vr, req, resp, NULL, 0, SCSI_DATA_NONE, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 10;        /* LBA 10 in 4K blocks */
    req->cdb[8] = 1;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, wbuf, 4096,
                                  SCSI_DATA_OUT, 1);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("write status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 10;
    req->cdb[8] = 1;
    memset(rbuf, 0, 4096);
    r = scsi_do_cmd(dev, vr, req, resp, rbuf, 4096, SCSI_DATA_IN, 2);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read status 0x%02x", resp->status);
    if (memcmp(wbuf, rbuf, 4096) != 0)
        TFAIL("read data does not match written data");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0232, VIRTIO_PCI_DEVICE_SCSI, test_scsi_wr_lun1,
                "A 4K block round trips on LUN 1",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

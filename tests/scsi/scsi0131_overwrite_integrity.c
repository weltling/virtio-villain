/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0131: overwrite_integrity
 *
 * Write one pattern to a block, overwrite it with a second pattern,
 * then read the block back and confirm the second pattern is returned.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_overwrite(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *a = vv_alloc_pages(1);
    uint8_t *b = vv_alloc_pages(1);
    uint8_t *rbuf = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++) {
        a[i] = (uint8_t)(i + 1);
        b[i] = (uint8_t)(0xC0 - i);
    }

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    for (int pass = 0; pass < 2; pass++) {
        memset(req, 0, sizeof(*req));
        scsi_set_lun(req->lun, 0, 0);
        req->cdb[0] = 0x2A;      /* WRITE(10) */
        req->cdb[5] = 200;
        req->cdb[8] = 1;
        uint8_t *src = pass == 0 ? a : b;
        test_result_t r = scsi_do_cmd(dev, vr, req, resp, src, 512,
                                      SCSI_DATA_OUT, seq++);
        if (r != TEST_PASS)
            return r;
        __sync_synchronize();
        if (resp->status != 0)
            TFAIL("write status 0x%02x", resp->status);
    }

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 200;
    req->cdb[8] = 1;
    memset(rbuf, 0, 512);
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, rbuf, 512,
                                  SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read status 0x%02x", resp->status);
    if (memcmp(b, rbuf, 512) != 0)
        TFAIL("read did not return the overwriting pattern");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0131, VIRTIO_PCI_DEVICE_SCSI, test_scsi_overwrite,
                "Overwriting a block returns the newer data",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

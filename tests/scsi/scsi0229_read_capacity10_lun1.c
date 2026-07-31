/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0229: read_capacity10_lun1
 *
 * READ CAPACITY(10) on LUN 1 reports its 4096-byte logical block size
 * and the matching last logical block address of 4095.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_rc10_lun1(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    /* Clear the power-on UNIT ATTENTION on LUN 1. */
    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x00;      /* TEST UNIT READY */
    scsi_do_cmd(dev, vr, req, resp, NULL, 0, SCSI_DATA_NONE, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 1);
    req->cdb[0] = 0x25;      /* READ CAPACITY(10) */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 8,
                                  SCSI_DATA_IN, 1);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (scsi_be32(data + 4) != 4096)
        TFAIL("block_len %u, expected 4096", scsi_be32(data + 4));
    if (scsi_be32(data) != 4095)
        TFAIL("last_lba %u, expected 4095", scsi_be32(data));
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0229, VIRTIO_PCI_DEVICE_SCSI, test_scsi_rc10_lun1,
                "READ CAPACITY(10) on LUN 1 reports a 4K block size",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

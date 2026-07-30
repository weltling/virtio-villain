/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0029: read_capacity16
 *
 * READ CAPACITY(16) returns a good status and a block length of 512 in
 * the returned parameter data.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read_capacity16(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x9E;      /* SERVICE ACTION IN(16) */
    req->cdb[1] = 0x10;      /* READ CAPACITY(16) */
    req->cdb[13] = 32;       /* allocation length */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 32,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    uint32_t block_len = scsi_be32(data + 8);
    if (block_len != 512)
        TFAIL("block_len %u, expected 512", block_len);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0029, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read_capacity16,
                "READ CAPACITY(16) reports a 512-byte block length",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

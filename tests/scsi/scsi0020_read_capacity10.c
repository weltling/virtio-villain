/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0020: read_capacity10
 *
 * READ CAPACITY(10) returns the last logical block address and the
 * block length. The backing LUN is a 16 MiB image of 512-byte blocks,
 * so the last LBA is 32767 and the block length is 512.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read_capacity10(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x25;      /* READ CAPACITY(10) */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 8,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    uint32_t last_lba = scsi_be32(data);
    uint32_t block_len = scsi_be32(data + 4);
    if (block_len != 512)
        TFAIL("block_len %u, expected 512", block_len);
    if (last_lba != 32767)
        TFAIL("last_lba %u, expected 32767", last_lba);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0020, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read_capacity10,
                "READ CAPACITY(10) reports the expected geometry",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0126: read_capacity10_pmi
 *
 * READ CAPACITY(10) with the partial medium indicator set returns a
 * good status and a 512-byte block length.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_rc10_pmi(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x25;      /* READ CAPACITY(10) */
    req->cdb[8] = 0x01;      /* partial medium indicator */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 8,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (scsi_be32(data + 4) != 512)
        TFAIL("block_len %u, expected 512", scsi_be32(data + 4));
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0126, VIRTIO_PCI_DEVICE_SCSI, test_scsi_rc10_pmi,
                "READ CAPACITY(10) with PMI returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

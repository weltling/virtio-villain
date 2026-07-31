/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0148: read_consistency
 *
 * Reading the same block twice returns identical data.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read_consistent(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *a = vv_alloc_pages(1);
    uint8_t *b = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 5;
    req->cdb[8] = 1;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, a, 512,
                                  SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("first read status 0x%02x", resp->status);

    r = scsi_do_cmd(dev, vr, req, resp, b, 512, SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("second read status 0x%02x", resp->status);
    if (memcmp(a, b, 512) != 0)
        TFAIL("two reads of the same block differ");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0148, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read_consistent,
                "Two reads of the same block return identical data",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

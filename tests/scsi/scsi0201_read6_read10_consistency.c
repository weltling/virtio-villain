/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0201: read6_read10_consistency
 *
 * Reading the same block with READ(6) and READ(10) returns identical
 * data.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read6_read10(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *a = vv_alloc_pages(1);
    uint8_t *b = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x08;      /* READ(6) */
    req->cdb[3] = 7;         /* LBA 7 */
    req->cdb[4] = 1;
    test_result_t r = scsi_do_cmd(dev, vr, req, resp, a, 512,
                                  SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read6 status 0x%02x", resp->status);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[5] = 7;
    req->cdb[8] = 1;
    r = scsi_do_cmd(dev, vr, req, resp, b, 512, SCSI_DATA_IN, seq++);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("read10 status 0x%02x", resp->status);
    if (memcmp(a, b, 512) != 0)
        TFAIL("READ(6) and READ(10) of the same block differ");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0201, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read6_read10,
                "READ(6) and READ(10) return the same block data",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

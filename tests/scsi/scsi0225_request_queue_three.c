/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0225: request_queue_three
 *
 * A READ(10) submitted on the second request queue completes with a
 * good status, confirming that request queue is serviced.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_reqq3(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 1;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 512,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0225, VIRTIO_PCI_DEVICE_SCSI, test_scsi_reqq3,
                "Second request queue services a READ",
                VIRTIO_SPEC_V1_4, "5.6.6.1", 3);

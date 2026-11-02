/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0002: read_lba0
 *
 * Baseline positive path. Issue a SCSI READ(10) of one block at LBA 0
 * on the request queue and confirm the device returns a completion
 * with response OK and a good SCSI status. Validates LUN addressing,
 * the CDB layout and the request/response header handling end to end.
 *
 * A SCSI target reports a power-on UNIT ATTENTION as a CHECK CONDITION
 * on its first command after reset, so the first READ clears it and
 * the second is checked for a good status.
 */
#include "lib/virtio_test.h"

static test_result_t submit_read(struct virtio_dev *dev, struct vring *vr,
                                 struct virtio_scsi_cmd_req *req,
                                 struct virtio_scsi_cmd_resp *resp,
                                 uint8_t *data, uint16_t seq)
{
    resp->response = 0xFF;
    resp->status = 0xFF;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            2);
    vv_desc(vr, 2, data, 512, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

static test_result_t test_scsi_read_lba0(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 0x01;      /* transfer length one block */

    /* First command clears the power-on UNIT ATTENTION. */
    test_result_t r = submit_read(dev, vr, req, resp, data, 0);
    if (r != TEST_PASS)
        return r;

    r = submit_read(dev, vr, req, resp, data, 1);
    if (r != TEST_PASS)
        return r;

    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x, expected OK", resp->response);
    if (resp->status != 0)
        TFAIL("scsi status 0x%02x, expected 0", resp->status);

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0002, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read_lba0,
                "READ(10) of LBA 0 completes with good status",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

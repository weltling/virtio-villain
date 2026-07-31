/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0085: request_header_split
 *
 * Scatter the command header across three readable descriptors and
 * confirm the device reassembles it and completes with a good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_header_split(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 1;
    resp->status = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    vring_raw_set_desc(vr, 0, req_phys, 20, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, req_phys + 20, 20, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, req_phys + 40, sizeof(*req) - 40,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0085, VIRTIO_PCI_DEVICE_SCSI, test_scsi_header_split,
                "Command header scattered across descriptors is reassembled",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0087: two_batched_reads
 *
 * Post two READ(10) requests before kicking and confirm the device
 * completes both.
 */
#include "lib/scsi_util.h"

static test_result_t test_scsi_two_reads(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req0 = vv_alloc_pages(1);
    struct virtio_scsi_cmd_req *req1 = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp0 = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp1 = vv_alloc_pages(1);
    uint8_t *data0 = vv_alloc_pages(1);
    uint8_t *data1 = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    scsi_set_lun(req0->lun, 0, 0);
    req0->cdb[0] = 0x28;
    req0->cdb[8] = 1;
    memcpy(req1, req0, sizeof(*req0));
    resp0->status = 0xFF;
    resp1->status = 0xFF;

    vv_desc(vr, 0, req0, sizeof(*req0), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp0, sizeof(*resp0),
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, data0, 512, VRING_DESC_F_WRITE, 0);
    vv_desc(vr, 3, req1, sizeof(*req1), VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, resp1, sizeof(*resp1),
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vv_desc(vr, 5, data1, 512, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail(vr, seq + 1, 3);
    vring_raw_set_avail_idx(vr, seq + 2);

    VV_TRY(vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS));
    __sync_synchronize();
    if (resp0->status != 0 || resp1->status != 0)
        TFAIL("status %02x %02x", resp0->status, resp1->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0087, VIRTIO_PCI_DEVICE_SCSI, test_scsi_two_reads,
                "Two batched READ requests both complete",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

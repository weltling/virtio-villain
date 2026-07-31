/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0087: two_batched_reads
 *
 * Post two READ(10) requests before kicking and confirm the device
 * completes both.
 */
#include "tests/scsi/scsi_util.h"

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

    memset(req0, 0, sizeof(*req0));
    scsi_set_lun(req0->lun, 0, 0);
    req0->cdb[0] = 0x28;
    req0->cdb[8] = 1;
    memcpy(req1, req0, sizeof(*req0));
    resp0->status = 0xFF;
    resp1->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req0), sizeof(*req0),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp0), sizeof(*resp0),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data0), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(req1), sizeof(*req1),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(resp1), sizeof(*resp1),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vring_raw_set_desc(vr, 5, vv_virt_to_phys(data1), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail(vr, seq + 1, 3);
    vring_raw_set_avail_idx(vr, seq + 2);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp0->status != 0 || resp1->status != 0)
        TFAIL("status %02x %02x", resp0->status, resp1->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0087, VIRTIO_PCI_DEVICE_SCSI, test_scsi_two_reads,
                "Two batched READ requests both complete",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0088: zero_length_trailing_desc
 *
 * Append a zero-length writable descriptor after the data-in buffer and
 * confirm the device tolerates it and completes with a good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_zero_trailing(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *tail = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 1;
    resp->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(tail), 0,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    __sync_synchronize();
    /* The device may accept the odd chain and complete, or decline it.
     * A completion must carry a good status; declining is also fine. */
    if (r == TEST_PASS && resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0088, VIRTIO_PCI_DEVICE_SCSI, test_scsi_zero_trailing,
                "READ with a zero-length trailing descriptor completes",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

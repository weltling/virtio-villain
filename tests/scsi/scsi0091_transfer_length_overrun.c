/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0091: transfer_length_overrun
 *
 * Request a transfer larger than the provided data-in buffer and
 * confirm the device reports an overrun rather than writing past the
 * buffer.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_overrun(struct virtio_dev *dev,
                                       struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 8;         /* eight blocks requested */
    resp->response = 0xFF;

    /* Data-in buffer holds only one block. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OVERRUN)
        TFAIL("response 0x%02x, expected overrun", resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0091, VIRTIO_PCI_DEVICE_SCSI, test_scsi_overrun,
                "Transfer larger than the buffer reports an overrun",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

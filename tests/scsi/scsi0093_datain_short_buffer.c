/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0093: datain_short_buffer
 *
 * Provide a data-in buffer one block short of the requested transfer
 * and confirm the device reports an overrun.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_datain_short(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 1;         /* one block, 512 bytes */
    resp->response = 0xFF;

    /* Data-in buffer is 256 bytes, short of the 512 transferred. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), 256,
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

REGISTER_TEST_Q(SCSI0093, VIRTIO_PCI_DEVICE_SCSI, test_scsi_datain_short,
                "READ into a short data-in buffer reports an overrun",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

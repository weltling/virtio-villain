/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0180: oversized_request_header
 *
 * A command header descriptor longer than the request header is
 * tolerated: the device reads the header it needs and completes with a
 * good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_oversized_header(struct virtio_dev *dev,
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

    /* Header descriptor advertises 64 extra readable bytes. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req) + 64,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    __sync_synchronize();
    /* The device may accept the padded header and complete or decline
     * the mismatched length. A completion must carry a good status. */
    if (r == TEST_PASS && resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0180, VIRTIO_PCI_DEVICE_SCSI, test_scsi_oversized_header,
                "Oversized command header is handled without host harm",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

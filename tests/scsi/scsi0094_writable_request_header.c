/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0094: writable_request_header
 *
 * Mark the command header descriptor writable, contradicting its role
 * as a device readable buffer. The device must refuse the request
 * without a completion; the host must stay alive.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_writable_header(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x00;      /* TEST UNIT READY */

    /* Command header descriptor wrongly marked writable. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a request with a writable header");

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0094, VIRTIO_PCI_DEVICE_SCSI, test_scsi_writable_header,
                "Writable command header is refused without host harm",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

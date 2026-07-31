/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0200: an_truncated_req
 *
 * Advertise the asynchronous notification request descriptor as far
 * shorter than a full request. The device must refuse it without a
 * completion; the host must stay alive.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_an_truncated(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_ctrl_an_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_an_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_SCSI_T_AN_QUERY;
    scsi_set_lun(req->lun, 0, 0);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), 4,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a truncated notification request");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0200, VIRTIO_PCI_DEVICE_SCSI, test_scsi_an_truncated,
                "Truncated notification request is refused safely",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

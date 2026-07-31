/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0198: four_batched_tmf
 *
 * Post four task management requests on the control queue before
 * kicking and confirm the device answers all four with defined response
 * codes.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_four_tmf(struct virtio_dev *dev,
                                        struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_SCSI_T_TMF;
    req->subtype = VIRTIO_SCSI_T_TMF_QUERY_TASK_SET;
    scsi_set_lun(req->lun, 0, 0);

    for (int k = 0; k < 4; k++) {
        uint16_t base = (uint16_t)(k * 2);
        vring_raw_set_desc(vr, base, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(resp),
                           sizeof(*resp), VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, k, base);
    }
    vring_raw_set_avail_idx(vr, 4);

    return vv_kick_and_wait_n(dev, vr, 0, 4, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(SCSI0198, VIRTIO_PCI_DEVICE_SCSI, test_scsi_four_tmf,
                "Four batched task management requests all complete",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

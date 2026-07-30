/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0005: tmf_lun_reset
 *
 * Issue a LOGICAL UNIT RESET task management function on the control
 * queue and confirm the device answers with a function complete
 * response.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_tmf_lun_reset(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_SCSI_T_TMF;
    req->subtype = VIRTIO_SCSI_T_TMF_LOGICAL_UNIT_RESET;
    req->lun[0] = 1;
    req->lun[2] = 0x40;

    resp->response = 0xAA;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_FUNCTION_COMPLETE &&
        resp->response != VIRTIO_SCSI_S_FUNCTION_SUCCEEDED)
        TFAIL("response 0x%02x, expected function complete", resp->response);

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0005, VIRTIO_PCI_DEVICE_SCSI, test_scsi_tmf_lun_reset,
                "LOGICAL UNIT RESET task management completes",
                VIRTIO_SPEC_V1_2, "5.6.6.2", 0);

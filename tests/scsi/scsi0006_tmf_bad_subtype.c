/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0006: tmf_bad_subtype
 *
 * Send a task management function with an out of range subtype on the
 * control queue and confirm the device answers with a function
 * rejected response instead of stalling or faulting.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_tmf_bad_subtype(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_SCSI_T_TMF;
    req->subtype = 0xdead;   /* not a defined TMF subtype */
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
    if (resp->response != VIRTIO_SCSI_S_FUNCTION_REJECTED)
        TFAIL("response 0x%02x, expected function rejected", resp->response);

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0006, VIRTIO_PCI_DEVICE_SCSI, test_scsi_tmf_bad_subtype,
                "Task management with an unknown subtype is rejected",
                VIRTIO_SPEC_V1_2, "5.6.6.2", 0);

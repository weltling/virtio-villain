/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0098: control_unknown_type
 *
 * Send a control queue request with an undefined type field. The device
 * must either refuse it or answer with a defined response code, and the
 * host must stay alive.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_control_unknown(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_ctrl_tmf_req *req = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->type = 0x5EED;      /* not a defined control request type */
    scsi_set_lun(req->lun, 0, 0);
    resp->response = 0xAA;

    /* Whether the device refuses the request or completes it, it must
     * stay operational afterwards. */
    scsi_do_tmf(dev, vr, req, resp, 0);
    __sync_synchronize();
    if (!(dev->common->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("device_status 0x%02x lost DRIVER_OK",
              dev->common->device_status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0098, VIRTIO_PCI_DEVICE_SCSI, test_scsi_control_unknown,
                "Control request with an unknown type is handled safely",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

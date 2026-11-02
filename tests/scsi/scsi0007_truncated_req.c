/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0007: truncated_req
 *
 * Submit a request whose command header descriptor is far shorter than
 * a full virtio_scsi_cmd_req, so the device reads a truncated header.
 * The host must stay alive; the queue may reject or wedge.
 */
#include "lib/virtio_test.h"

static test_result_t test_scsi_truncated_req(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->cdb[0] = 0x28;

    /* Command header advertised as only 8 bytes, well short of the real
     * header, so the device gathers a truncated request. */
    vv_desc(vr, 0, req, 8, VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            2);
    vv_desc(vr, 2, data, 512, VRING_DESC_F_WRITE, 0);

    vring_submit(vr, 0);

    /* A truncated request must not be turned into a completion. QEMU
     * drops it and stops the queue; the host stays alive and the guest
     * reaches this assertion. */
    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a truncated request");

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0007, VIRTIO_PCI_DEVICE_SCSI, test_scsi_truncated_req,
                "Truncated request header is handled without host harm",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

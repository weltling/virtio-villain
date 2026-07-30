/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0009: no_writable_resp
 *
 * Submit a request whose entire descriptor chain is read only, leaving
 * the device no writable buffer for the response header. The device
 * must not write into a read only descriptor; the host must stay alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_no_writable_resp(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    req->cdb[0] = 0x00;      /* TEST UNIT READY */

    /* Response descriptor left read only, so there is no writable
     * buffer for the completion header. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp), 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* With no writable buffer the device cannot post a response. It
     * refuses the request without a completion; the host stays alive. */
    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a request with no writable buffer");

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0009, VIRTIO_PCI_DEVICE_SCSI, test_scsi_no_writable_resp,
                "Request with no writable response buffer is handled safely",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

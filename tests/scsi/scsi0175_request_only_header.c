/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0175: request_only_header
 *
 * A request chain that carries only the command header with no writable
 * response descriptor is refused without a completion; the host must
 * stay alive.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_only_header(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x00;      /* TEST UNIT READY */

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req), 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a request with no response buffer");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0175, VIRTIO_PCI_DEVICE_SCSI, test_scsi_only_header,
                "Request with only a header is refused safely",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

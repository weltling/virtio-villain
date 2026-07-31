/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0147: test_unit_ready_idempotent
 *
 * TEST UNIT READY issued twice after the power-on UNIT ATTENTION is
 * cleared returns a good status each time.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_tur_twice(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    for (int k = 0; k < 2; k++) {
        memset(req, 0, sizeof(*req));
        scsi_set_lun(req->lun, 0, 0);
        req->cdb[0] = 0x00;      /* TEST UNIT READY */
        test_result_t r = scsi_do_cmd(dev, vr, req, resp, NULL, 0,
                                      SCSI_DATA_NONE, seq++);
        if (r != TEST_PASS)
            return r;
        __sync_synchronize();
        if (resp->status != 0)
            TFAIL("status 0x%02x on pass %d", resp->status, k);
    }
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0147, VIRTIO_PCI_DEVICE_SCSI, test_scsi_tur_twice,
                "TEST UNIT READY is idempotent",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

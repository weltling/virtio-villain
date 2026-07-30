/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0030: synchronize_cache
 *
 * SYNCHRONIZE CACHE(10) returns a good status once the power-on UNIT
 * ATTENTION is cleared.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_sync_cache(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x35;      /* SYNCHRONIZE CACHE(10) */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, NULL, 0,
                                  SCSI_DATA_NONE, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0030, VIRTIO_PCI_DEVICE_SCSI, test_scsi_sync_cache,
                "SYNCHRONIZE CACHE(10) returns good status",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

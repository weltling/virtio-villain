/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0119: read12_multiblock
 *
 * READ(12) of eight consecutive blocks returns a good status once the
 * power-on UNIT ATTENTION is cleared.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read12_multi(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xA8;      /* READ(12) */
    req->cdb[9] = 8;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 8 * 512,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0119, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read12_multi,
                "READ(12) of eight blocks returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

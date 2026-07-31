/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0195: read12_32blocks
 *
 * READ(12) of thirty two consecutive blocks returns a good status once
 * the power-on UNIT ATTENTION is cleared.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read12_32(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(4);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0xA8;      /* READ(12) */
    req->cdb[9] = 32;

    test_result_t r = scsi_do_cmd_paged(dev, vr, req, resp, data, 32 * 512,
                                        SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0195, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read12_32,
                "READ(12) of thirty two blocks returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

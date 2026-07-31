/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0184: read10_rdprotect
 *
 * A READ(10) that requests protection information from a disk without
 * protection is answered with a defined status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_read10_rdprotect(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[1] = 0x20;      /* RDPROTECT field nonzero */
    req->cdb[8] = 1;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 512,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (!scsi_status_defined(resp->status))
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0184, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read10_rdprotect,
                "READ(10) requesting protection returns a defined status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

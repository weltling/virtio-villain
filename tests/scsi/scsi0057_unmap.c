/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0057: unmap
 *
 * UNMAP with a single block descriptor returns a defined completion
 * status, exercising the discard path.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_unmap(struct virtio_dev *dev, struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    /* UNMAP parameter list: 8-byte header then one 16-byte descriptor. */
    memset(data, 0, 24);
    data[0] = 0; data[1] = 22;   /* unmap data length */
    data[2] = 0; data[3] = 16;   /* block descriptor data length */
    data[8 + 7] = 250;           /* descriptor LBA 250 (low byte) */
    data[8 + 11] = 1;            /* number of logical blocks */

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x42;      /* UNMAP */
    req->cdb[8] = 24;        /* parameter list length */

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 24,
                                  SCSI_DATA_OUT, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (!scsi_status_defined(resp->status))
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0057, VIRTIO_PCI_DEVICE_SCSI, test_scsi_unmap,
                "UNMAP of one block returns a defined status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

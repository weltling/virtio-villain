/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0109: inquiry_product_id
 *
 * The standard INQUIRY product identification field is populated rather
 * than left blank.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inq_product(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 96;
    memset(data, 0x20, 96);

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 96,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    int nonblank = 0;
    for (int i = 16; i < 32; i++)
        if (data[i] != 0x20 && data[i] != 0x00)
            nonblank = 1;
    if (!nonblank)
        TFAIL("product id is blank");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0109, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inq_product,
                "INQUIRY product identification is populated",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

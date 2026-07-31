/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0190: unmap_two_descriptors
 *
 * UNMAP with two block descriptors returns a defined status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_unmap_two(struct virtio_dev *dev,
                                         struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    /* Header plus two 16-byte block descriptors. */
    memset(data, 0, 40);
    data[0] = 0; data[1] = 38;   /* unmap data length */
    data[2] = 0; data[3] = 32;   /* block descriptor data length */
    data[8 + 7] = 60;            /* first descriptor LBA */
    data[8 + 11] = 1;            /* first descriptor block count */
    data[24 + 7] = 70;           /* second descriptor LBA */
    data[24 + 11] = 1;           /* second descriptor block count */

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x42;      /* UNMAP */
    req->cdb[8] = 40;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 40,
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

REGISTER_TEST_Q(SCSI0190, VIRTIO_PCI_DEVICE_SCSI, test_scsi_unmap_two,
                "UNMAP with two descriptors returns a defined status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0108: inquiry_vendor_id
 *
 * The standard INQUIRY vendor identification field carries the QEMU
 * vendor string.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inq_vendor(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 96;
    memset(data, 0, 96);

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 96,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    if (memcmp(data + 8, "QEMU", 4) != 0)
        TFAIL("vendor id not QEMU");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0108, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inq_vendor,
                "INQUIRY vendor identification is QEMU",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

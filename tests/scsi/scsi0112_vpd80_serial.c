/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0112: vpd80_serial
 *
 * The unit serial number VPD page 0x80 carries the serial configured on
 * the backing disk, which starts with the letter v.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_vpd80_serial(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0x80;      /* unit serial number */
    req->cdb[4] = 64;
    memset(data, 0, 64);

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 64,
                                  SCSI_DATA_IN, 0);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    unsigned page_len = data[3];
    if (page_len == 0)
        TFAIL("serial page length is zero");
    if (data[4] != 'v')
        TFAIL("serial starts with 0x%02x, expected 'v'", data[4]);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0112, VIRTIO_PCI_DEVICE_SCSI, test_scsi_vpd80_serial,
                "Unit serial number page carries the configured serial",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

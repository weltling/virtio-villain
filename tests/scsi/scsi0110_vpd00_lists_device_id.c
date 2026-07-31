/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0110: vpd00_lists_device_id
 *
 * The supported VPD pages list returned for page 0x00 includes the
 * device identification page 0x83.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_vpd00_lists_83(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[1] = 0x01;      /* EVPD */
    req->cdb[2] = 0x00;      /* supported VPD pages */
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
    int found = 0;
    for (unsigned i = 0; i < page_len && (4 + i) < 64; i++)
        if (data[4 + i] == 0x83)
            found = 1;
    if (!found)
        TFAIL("page 0x83 not in supported list");
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0110, VIRTIO_PCI_DEVICE_SCSI, test_scsi_vpd00_lists_83,
                "Supported VPD pages list includes device identification",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

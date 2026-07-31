/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0138: write16_fua_dpo
 *
 * WRITE(16) with both the force unit access and disable page out bits
 * returns a good status once the power-on UNIT ATTENTION is cleared.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_write16_fua_dpo(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++)
        data[i] = (uint8_t)(i + 29);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x8A;      /* WRITE(16) */
    req->cdb[1] = 0x18;      /* force unit access and disable page out */
    req->cdb[9] = 120;       /* LBA */
    req->cdb[13] = 1;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 512,
                                  SCSI_DATA_OUT, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0138, VIRTIO_PCI_DEVICE_SCSI, test_scsi_write16_fua_dpo,
                "WRITE(16) with FUA and DPO returns good status",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

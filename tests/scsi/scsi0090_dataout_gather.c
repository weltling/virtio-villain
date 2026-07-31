/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0090: dataout_gather
 *
 * Scatter the WRITE(10) data-out payload across two readable
 * descriptors and confirm the device gathers it and completes with a
 * good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_dataout_gather(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *wbuf = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++)
        wbuf[i] = (uint8_t)(i + 17);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 80;
    req->cdb[8] = 1;
    resp->status = 0xFF;

    uint64_t wphys = vv_virt_to_phys(wbuf);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, wphys, 256, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, wphys + 256, 256, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0090, VIRTIO_PCI_DEVICE_SCSI, test_scsi_dataout_gather,
                "WRITE with a scattered data-out payload completes",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

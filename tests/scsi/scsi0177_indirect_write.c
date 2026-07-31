/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0177: indirect_write
 *
 * Submit a WRITE(10) through an indirect descriptor table and confirm
 * the device follows the table and completes with a good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_indirect_write(struct virtio_dev *dev,
                                              struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        TSKIP("indirect descriptors not offered");

    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    struct vring_desc *tbl = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    for (int i = 0; i < 512; i++)
        data[i] = (uint8_t)(i + 41);
    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 35;
    req->cdb[8] = 1;
    resp->status = 0xFF;

    tbl[0].addr = vv_virt_to_phys(req);
    tbl[0].len = sizeof(*req);
    tbl[0].flags = VRING_DESC_F_NEXT;
    tbl[0].next = 1;
    tbl[1].addr = vv_virt_to_phys(data);
    tbl[1].len = 512;
    tbl[1].flags = VRING_DESC_F_NEXT;
    tbl[1].next = 2;
    tbl[2].addr = vv_virt_to_phys(resp);
    tbl[2].len = sizeof(*resp);
    tbl[2].flags = VRING_DESC_F_WRITE;
    tbl[2].next = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(tbl),
                       3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT, 0);
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

REGISTER_TEST_Q_REQUIRES(SCSI0177, VIRTIO_PCI_DEVICE_SCSI,
                         test_scsi_indirect_write,
                         "WRITE through an indirect descriptor table",
                         VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST,
                         VV_FEATURE_BIT(VIRTIO_F_INDIRECT_DESC), 0);

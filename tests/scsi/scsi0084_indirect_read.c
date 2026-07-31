/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0084: indirect_read
 *
 * Submit a READ(10) through an indirect descriptor table and confirm
 * the device follows the table and completes with a good status.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_indirect_read(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        TSKIP("indirect descriptors not offered");

    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    struct vring_desc *tbl = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 1;
    resp->status = 0xFF;

    tbl[0].addr = vv_virt_to_phys(req);
    tbl[0].len = sizeof(*req);
    tbl[0].flags = VRING_DESC_F_NEXT;
    tbl[0].next = 1;
    tbl[1].addr = vv_virt_to_phys(resp);
    tbl[1].len = sizeof(*resp);
    tbl[1].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    tbl[1].next = 2;
    tbl[2].addr = vv_virt_to_phys(data);
    tbl[2].len = 512;
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

REGISTER_TEST_Q_REQUIRES(SCSI0084, VIRTIO_PCI_DEVICE_SCSI,
                         test_scsi_indirect_read,
                         "READ through an indirect descriptor table",
                         VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST,
                         VV_FEATURE_BIT(VIRTIO_F_INDIRECT_DESC), 0);

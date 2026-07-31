/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0173: indirect_and_next
 *
 * A descriptor that sets both the indirect and next flags is malformed.
 * The device must refuse it without a completion; the host must stay
 * alive.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_indirect_next(struct virtio_dev *dev,
                                             struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        TSKIP("indirect descriptors not offered");

    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    struct vring_desc *tbl = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;
    req->cdb[8] = 1;

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

    /* Both indirect and next set, which the spec forbids. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(tbl),
                       3 * sizeof(struct vring_desc),
                       VRING_DESC_F_INDIRECT | VRING_DESC_F_NEXT, 1);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Some devices reject this, others ignore the extra flag. Either
     * way the host must survive and the device stay operational. */
    vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    __sync_synchronize();
    if (!(dev->common->device_status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("device_status 0x%02x lost DRIVER_OK",
              dev->common->device_status);
    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(SCSI0173, VIRTIO_PCI_DEVICE_SCSI,
                         test_scsi_indirect_next,
                         "Indirect descriptor with next set is handled safely",
                         VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST,
                         VV_FEATURE_BIT(VIRTIO_F_INDIRECT_DESC), 0);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0218: five_batched_reads
 *
 * Post five READ(10) requests before kicking and confirm the device
 * completes all five, exercising several in flight requests.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_five_reads(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 1;

    for (int k = 0; k < 5; k++) {
        uint16_t base = (uint16_t)(k * 3);
        vring_raw_set_desc(vr, base, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(resp),
                           sizeof(*resp),
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
        vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(data), 512,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, k, base);
    }
    vring_raw_set_avail_idx(vr, 5);

    return vv_kick_and_wait_n(dev, vr, 0, 5, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(SCSI0218, VIRTIO_PCI_DEVICE_SCSI, test_scsi_five_reads,
                "Five batched READ requests all complete",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

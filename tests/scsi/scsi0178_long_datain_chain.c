/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0178: long_datain_chain
 *
 * READ(10) of six blocks into a data-in buffer split across twelve
 * descriptors returns a good status once the power-on UNIT ATTENTION is
 * cleared.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_long_chain(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 6;         /* six blocks, 3072 bytes */
    resp->status = 0xFF;

    uint64_t dphys = vv_virt_to_phys(data);
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    for (int i = 0; i < 12; i++) {
        uint16_t d = (uint16_t)(2 + i);
        uint16_t flags = VRING_DESC_F_WRITE;
        uint16_t next = 0;
        if (i != 11) {
            flags |= VRING_DESC_F_NEXT;
            next = (uint16_t)(d + 1);
        }
        vring_raw_set_desc(vr, d, dphys + i * 256, 256, flags, next);
    }
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

REGISTER_TEST_Q(SCSI0178, VIRTIO_PCI_DEVICE_SCSI, test_scsi_long_chain,
                "READ into a twelve descriptor chain completes",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

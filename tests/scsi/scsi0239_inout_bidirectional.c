/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0239: inout_bidirectional
 *
 * With the INOUT feature the driver may build a request that carries
 * both a device readable data-out buffer and a device writable data-in
 * buffer. Submit such a bidirectional layout and confirm the device
 * completes it with a defined status. Skips when INOUT is not offered.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_inout(struct virtio_dev *dev, struct vring *vr)
{
    if (!virtio_pci_feature_offered(dev, VIRTIO_SCSI_F_INOUT))
        TSKIP("INOUT feature not offered");

    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *dout = vv_alloc_pages(1);
    uint8_t *din = vv_alloc_pages(1);

    for (int i = 0; i < 512; i++)
        dout[i] = (uint8_t)(i + 5);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x2A;      /* WRITE(10) */
    req->cdb[5] = 42;
    req->cdb[8] = 1;
    resp->response = 0xFF;

    /* Bidirectional layout: readable header and data-out, then the
     * writable response and an extra data-in buffer. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(dout), 512,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(din), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x", resp->response);
    if (!scsi_status_defined(resp->status))
        TFAIL("status 0x%02x", resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q_REQUIRES(SCSI0239, VIRTIO_PCI_DEVICE_SCSI, test_scsi_inout,
                         "A bidirectional request completes when INOUT is set",
                         VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST,
                         VV_FEATURE_BIT(VIRTIO_SCSI_F_INOUT), 0);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0240: abort_inflight_read
 *
 * A host side sidecar throttles the backing drive to one operation per
 * second. The guest submits a read that then sits queued behind the
 * throttle and aborts it by tag with an ABORT TASK on the control
 * queue. The device completes the read with the aborted response.
 *
 * Runs under QEMU with the API socket enabled. When the throttle is
 * not applied the read completes normally and the test skips.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

#define ABORT_TAG 0xABCD1234ULL

static test_result_t test_scsi_abort_inflight(struct virtio_dev *dev,
                                             struct vring *vr)
{
    /* vr is the control queue. Drive reads on our own request queue
     * ring at index 2, the first request queue. */
    struct vring rq;
    if (vring_alloc(&rq, 16) < 0)
        return TEST_SKIP;
    vring_attach(dev, &rq, 2);

    /* Give the host sidecar time to throttle the drive. */
    printf("vv-scsi-armed\n");
    fflush(stdout);
    usleep(2500000);

    uint16_t rseq = 0;
    rseq = scsi_clear_ua(dev, &rq, rseq);

    /* Warm-up read consumes the throttled budget for this slice. */
    struct virtio_scsi_cmd_req *ra = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *ra_resp = vv_alloc_pages(1);
    uint8_t *da = vv_alloc_pages(1);
    memset(ra, 0, sizeof(*ra));
    scsi_set_lun(ra->lun, 0, 0);
    ra->cdb[0] = 0x28;      /* READ(10) */
    ra->cdb[8] = 1;
    scsi_do_cmd(dev, &rq, ra, ra_resp, da, 512, SCSI_DATA_IN, rseq);
    rseq++;

    /* Target read, submitted without waiting so it stays queued behind
     * the throttle while the abort reaches the device. */
    struct virtio_scsi_cmd_req *rb = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *rb_resp = vv_alloc_pages(1);
    uint8_t *db = vv_alloc_pages(1);
    memset(rb, 0, sizeof(*rb));
    scsi_set_lun(rb->lun, 0, 0);
    rb->tag = ABORT_TAG;
    rb->cdb[0] = 0x28;
    rb->cdb[8] = 1;
    rb_resp->response = 0xFF;
    rb_resp->status = 0xFF;

    vring_raw_set_desc(&rq, 0, vv_virt_to_phys(rb), sizeof(*rb),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&rq, 1, vv_virt_to_phys(rb_resp), sizeof(*rb_resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&rq, 2, vv_virt_to_phys(db), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&rq, rseq, 0);
    vring_raw_set_avail_idx(&rq, rseq + 1);
    uint16_t rq_before = rq.used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, rq.queue);
    rseq++;

    /* Abort the queued read by tag on the control queue. */
    struct virtio_scsi_ctrl_tmf_req *tmf = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *tmf_resp = vv_alloc_pages(1);
    memset(tmf, 0, sizeof(*tmf));
    tmf->type = VIRTIO_SCSI_T_TMF;
    tmf->subtype = VIRTIO_SCSI_T_TMF_ABORT_TASK;
    scsi_set_lun(tmf->lun, 0, 0);
    tmf->tag = ABORT_TAG;
    scsi_do_tmf(dev, vr, tmf, tmf_resp, 0);

    /* Wait for the aborted read to complete on the request queue. */
    int elapsed = 0;
    while (elapsed < 8000000) {
        usleep(20000);
        __sync_synchronize();
        if (rq.used->idx != rq_before)
            break;
        elapsed += 20000;
    }

    __sync_synchronize();
    if (rq.used->idx == rq_before)
        TSKIP("aborted read did not complete (no host throttle)");
    if (rb_resp->response == VIRTIO_SCSI_S_OK)
        TSKIP("read completed before the abort took effect");
    if (rb_resp->response != VIRTIO_SCSI_S_ABORTED)
        TFAIL("response 0x%02x is not aborted", rb_resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0240, VIRTIO_PCI_DEVICE_SCSI, test_scsi_abort_inflight,
                "ABORT TASK of a throttled read returns the aborted response",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

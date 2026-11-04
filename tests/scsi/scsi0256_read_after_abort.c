/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0256: read_after_abort
 *
 * A host side sidecar throttles the drive so a guest read stays
 * queued. After aborting it by tag the guest issues a fresh read on
 * the same queue, which completes normally, confirming the device
 * keeps serving after an abort. Skips when the throttle is not
 * applied.
 */
#include "lib/scsi_util.h"

#define ABORT_TAG 0x56560001ULL

static test_result_t test_scsi_read_after_abort(struct virtio_dev *dev,
                                                struct vring *vr)
{
    struct vring rq;
    if (vring_alloc(&rq, 16) < 0)
        return TEST_SKIP;
    scsi_attach_request_queue(dev, vr, &rq, 2);

    printf("vv-scsi-armed\n");
    fflush(stdout);
    usleep(2500000);

    uint16_t rseq = 0;
    rseq = scsi_clear_ua(dev, &rq, rseq);

    struct virtio_scsi_cmd_req *wa = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *wr = vv_alloc_pages(1);
    uint8_t *wd = vv_alloc_pages(1);
    scsi_set_lun(wa->lun, 0, 0);
    wa->cdb[0] = 0x28;
    wa->cdb[8] = 1;
    scsi_do_cmd(dev, &rq, wa, wr, wd, 512, SCSI_DATA_IN, rseq);
    rseq++;

    struct virtio_scsi_cmd_req *rb = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *rb_resp = vv_alloc_pages(1);
    uint8_t *db = vv_alloc_pages(1);
    scsi_set_lun(rb->lun, 0, 0);
    rb->tag = ABORT_TAG;
    rb->cdb[0] = 0x28;
    rb->cdb[8] = 1;
    uint16_t before = scsi_submit_async(dev, &rq, rb, rb_resp, db, 512,
                                        SCSI_DATA_IN, rseq);
    rseq++;

    struct virtio_scsi_ctrl_tmf_req *tmf = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *tmf_resp = vv_alloc_pages(1);
    tmf->type = VIRTIO_SCSI_T_TMF;
    tmf->subtype = VIRTIO_SCSI_T_TMF_ABORT_TASK;
    scsi_set_lun(tmf->lun, 0, 0);
    tmf->tag = ABORT_TAG;
    VV_TRY(scsi_tmf_when_pending( dev, vr, rb, rb_resp, tmf, tmf_resp));

    if (!scsi_wait_used(&rq, before, 8000000))
        TSKIP("read did not complete (no host throttle)");
    if (rb_resp->response == VIRTIO_SCSI_S_OK)
        TSKIP("read completed before the abort took effect");
    if (rb_resp->response != VIRTIO_SCSI_S_ABORTED)
        TFAIL("aborted read response 0x%02x is not aborted", rb_resp->response);

    /* A fresh read must still complete normally. */
    struct virtio_scsi_cmd_req *rc = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *rc_resp = vv_alloc_pages(1);
    uint8_t *dc = vv_alloc_pages(1);
    scsi_set_lun(rc->lun, 0, 0);
    rc->cdb[0] = 0x28;
    rc->cdb[8] = 1;
    uint16_t before2 = scsi_submit_async(dev, &rq, rc, rc_resp, dc, 512,
                                         SCSI_DATA_IN, rseq);
    rseq++;
    if (!scsi_wait_used(&rq, before2, 8000000))
        TFAIL("fresh read did not complete after the abort");
    if (rc_resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("fresh read response 0x%02x is not ok", rc_resp->response);
    if (!scsi_status_defined(rc_resp->status))
        TFAIL("fresh read status 0x%02x", rc_resp->status);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0256, VIRTIO_PCI_DEVICE_SCSI, test_scsi_read_after_abort,
                "A fresh read completes after an aborted read",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

REGISTER_SCSI_MMIO(SCSI0256, test_scsi_read_after_abort, 0, 0);

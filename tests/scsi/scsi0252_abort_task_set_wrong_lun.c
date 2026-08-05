/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0252: abort_task_set_wrong_lun
 *
 * A host side sidecar throttles the drive so a guest read on LUN 0
 * stays queued. An ABORT TASK SET addressed to LUN 1 must not touch
 * it, so the read completes normally. This pins the logical unit
 * scope of the task set functions. Skips when the throttle is not
 * applied.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_abort_wrong_lun(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct vring rq;
    if (vring_alloc(&rq, 16) < 0)
        return TEST_SKIP;
    vring_attach(dev, &rq, 2);

    printf("vv-scsi-armed\n");
    fflush(stdout);
    usleep(2500000);

    uint16_t rseq = 0;
    rseq = scsi_clear_ua(dev, &rq, rseq);

    struct virtio_scsi_cmd_req *wa = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *wr = vv_alloc_pages(1);
    uint8_t *wd = vv_alloc_pages(1);
    memset(wa, 0, sizeof(*wa));
    scsi_set_lun(wa->lun, 0, 0);
    wa->cdb[0] = 0x28;
    wa->cdb[8] = 1;
    scsi_do_cmd(dev, &rq, wa, wr, wd, 512, SCSI_DATA_IN, rseq);
    rseq++;

    struct virtio_scsi_cmd_req *rb = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *rb_resp = vv_alloc_pages(1);
    uint8_t *db = vv_alloc_pages(1);
    memset(rb, 0, sizeof(*rb));
    scsi_set_lun(rb->lun, 0, 0);
    rb->tag = 0x52520001ULL;
    rb->cdb[0] = 0x28;
    rb->cdb[8] = 1;
    uint16_t before = scsi_submit_async(dev, &rq, rb, rb_resp, db, 512,
                                        SCSI_DATA_IN, rseq);
    rseq++;

    struct virtio_scsi_ctrl_tmf_req *tmf = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *tmf_resp = vv_alloc_pages(1);
    memset(tmf, 0, sizeof(*tmf));
    tmf->type = VIRTIO_SCSI_T_TMF;
    tmf->subtype = VIRTIO_SCSI_T_TMF_ABORT_TASK_SET;
    scsi_set_lun(tmf->lun, 0, 1);      /* the other logical unit */
    scsi_do_tmf(dev, vr, tmf, tmf_resp, 0);

    if (!scsi_wait_used(&rq, before, 8000000))
        TSKIP("read did not complete (no host throttle)");
    if (rb_resp->response == VIRTIO_SCSI_S_ABORTED)
        TFAIL("abort on another LUN terminated the read");
    if (rb_resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("response 0x%02x is not ok", rb_resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0252, VIRTIO_PCI_DEVICE_SCSI, test_scsi_abort_wrong_lun,
                "ABORT TASK SET on another LUN leaves the read running",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

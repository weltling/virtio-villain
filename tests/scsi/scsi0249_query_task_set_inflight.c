/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0249: query_task_set_inflight
 *
 * A host side sidecar throttles the drive so a guest read stays
 * queued. A QUERY TASK SET on the control queue reports the logical
 * unit as having a pending task. Skips when the throttle is not
 * applied or the device does not distinguish the query.
 */
#include "tests/scsi/scsi_util.h"

#define QUERY_TAG 0x49490001ULL

static test_result_t test_scsi_query_task_set(struct virtio_dev *dev,
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
    rb->tag = QUERY_TAG;
    rb->cdb[0] = 0x28;
    rb->cdb[8] = 1;
    scsi_submit_async(dev, &rq, rb, rb_resp, db, 512, SCSI_DATA_IN, rseq);
    rseq++;
    usleep(200000);

    struct virtio_scsi_ctrl_tmf_req *tmf = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *tmf_resp = vv_alloc_pages(1);
    memset(tmf, 0, sizeof(*tmf));
    tmf->type = VIRTIO_SCSI_T_TMF;
    tmf->subtype = VIRTIO_SCSI_T_TMF_QUERY_TASK_SET;
    scsi_set_lun(tmf->lun, 0, 0);
    tmf->tag = QUERY_TAG;
    scsi_do_tmf(dev, vr, tmf, tmf_resp, 0);

    if (tmf_resp->response == VIRTIO_SCSI_S_FUNCTION_SUCCEEDED)
        return TEST_PASS;
    if (!scsi_tmf_response_valid(tmf_resp->response))
        TFAIL("query task set response 0x%02x is undefined",
              tmf_resp->response);
    TSKIP("device does not report a pending task in the set");
}

REGISTER_TEST_Q(SCSI0249, VIRTIO_PCI_DEVICE_SCSI, test_scsi_query_task_set,
                "QUERY TASK SET reports a throttled read as pending",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

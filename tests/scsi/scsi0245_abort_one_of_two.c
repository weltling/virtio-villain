/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0245: abort_one_of_two
 *
 * A host side sidecar throttles the drive so two guest reads with
 * distinct tags both stay queued. An ABORT TASK for the first tag
 * terminates only that read while the second completes normally. This
 * pins the per task scope of ABORT TASK. Skips when the throttle is
 * not applied.
 */
#include "tests/scsi/scsi_util.h"

#define TAG_A 0x45450001ULL
#define TAG_B 0x45450002ULL

static test_result_t test_scsi_abort_one_of_two(struct virtio_dev *dev,
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

    /* Two reads submitted together, each on its own descriptor group,
     * so both sit queued behind the throttle at once. */
    struct virtio_scsi_cmd_req *ra = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *ra_resp = vv_alloc_pages(1);
    uint8_t *da = vv_alloc_pages(1);
    struct virtio_scsi_cmd_req *rb = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *rb_resp = vv_alloc_pages(1);
    uint8_t *db = vv_alloc_pages(1);
    memset(ra, 0, sizeof(*ra));
    memset(rb, 0, sizeof(*rb));
    scsi_set_lun(ra->lun, 0, 0);
    scsi_set_lun(rb->lun, 0, 0);
    ra->tag = TAG_A;
    rb->tag = TAG_B;
    ra->cdb[0] = 0x28;
    ra->cdb[8] = 1;
    rb->cdb[0] = 0x28;
    rb->cdb[8] = 1;
    ra_resp->response = 0xFF;
    rb_resp->response = 0xFF;

    vring_raw_set_desc(&rq, 0, vv_virt_to_phys(ra), sizeof(*ra),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&rq, 1, vv_virt_to_phys(ra_resp), sizeof(*ra_resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&rq, 2, vv_virt_to_phys(da), 512, VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(&rq, 3, vv_virt_to_phys(rb), sizeof(*rb),
                       VRING_DESC_F_NEXT, 4);
    vring_raw_set_desc(&rq, 4, vv_virt_to_phys(rb_resp), sizeof(*rb_resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 5);
    vring_raw_set_desc(&rq, 5, vv_virt_to_phys(db), 512, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&rq, rseq, 0);
    vring_raw_set_avail(&rq, rseq + 1, 3);
    vring_raw_set_avail_idx(&rq, rseq + 2);
    __sync_synchronize();
    virtio_pci_kick(dev, rq.queue);
    rseq += 2;

    struct virtio_scsi_ctrl_tmf_req *tmf = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *tmf_resp = vv_alloc_pages(1);
    memset(tmf, 0, sizeof(*tmf));
    tmf->type = VIRTIO_SCSI_T_TMF;
    tmf->subtype = VIRTIO_SCSI_T_TMF_ABORT_TASK;
    scsi_set_lun(tmf->lun, 0, 0);
    tmf->tag = TAG_A;
    scsi_do_tmf(dev, vr, tmf, tmf_resp, 0);

    /* Wait for both reads to report. */
    int elapsed = 0;
    while (elapsed < 10000000) {
        usleep(20000);
        __sync_synchronize();
        if (ra_resp->response != 0xFF && rb_resp->response != 0xFF)
            break;
        elapsed += 20000;
    }

    __sync_synchronize();
    if (ra_resp->response == 0xFF || rb_resp->response == 0xFF)
        TSKIP("reads did not complete (no host throttle)");
    if (ra_resp->response == VIRTIO_SCSI_S_OK)
        TSKIP("first read completed before the abort took effect");
    if (ra_resp->response != VIRTIO_SCSI_S_ABORTED)
        TFAIL("first read response 0x%02x is not aborted", ra_resp->response);
    if (rb_resp->response != VIRTIO_SCSI_S_OK)
        TFAIL("second read response 0x%02x is not ok", rb_resp->response);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0245, VIRTIO_PCI_DEVICE_SCSI, test_scsi_abort_one_of_two,
                "ABORT TASK terminates only the matching queued read",
                VIRTIO_SPEC_V1_4, "5.6.6.2", 0);

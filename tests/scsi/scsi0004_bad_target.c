/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0004: bad_target
 *
 * Address a target that has no attached logical unit and confirm the
 * device completes the request with a BAD_TARGET response rather than
 * stalling or faulting.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_bad_target(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->lun[0] = 1;
    req->lun[1] = 5;         /* target with no logical unit */
    req->lun[2] = 0x40;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    req->cdb[0] = 0x12;      /* INQUIRY */
    req->cdb[4] = 36;

    resp->response = 0xAA;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), 36,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    __sync_synchronize();
    if (resp->response != VIRTIO_SCSI_S_BAD_TARGET)
        TFAIL("response 0x%02x, expected BAD_TARGET", resp->response);

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0004, VIRTIO_PCI_DEVICE_SCSI, test_scsi_bad_target,
                "Request to an absent target returns BAD_TARGET",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

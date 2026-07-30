/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0010: cyclic_chain
 *
 * Post a request whose descriptor chain forms a cycle, so a naive
 * walker would loop forever. The device must bound the walk and keep
 * the host alive; the queue may reject or wedge.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_cyclic_chain(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->cdb[0] = 0x00;      /* TEST UNIT READY */

    /* desc0 -> desc1 -> desc0, a cycle. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* The device must bound the descriptor walk. It refuses the cyclic
     * chain without a completion; the host stays alive. */
    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) == TEST_PASS)
        TFAIL("device completed a cyclic descriptor chain");

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0010, VIRTIO_PCI_DEVICE_SCSI, test_scsi_cyclic_chain,
                "Cyclic descriptor chain is bounded without host harm",
                VIRTIO_SPEC_V1_2, "2.7.5", VV_QUEUE_LAST);

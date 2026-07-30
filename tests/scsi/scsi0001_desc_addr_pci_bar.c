/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0001: desc_addr_in_pci_bar
 *
 * Issue a SCSI READ on the request queue whose data-in descriptor
 * points into the device's own PCI BAR MMIO region instead of guest
 * RAM. The device must not DMA the returned block into its own
 * registers when it completes the request. This mirrors the block
 * device case (T0063) for the virtio-scsi request path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_desc_addr_pci_bar(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    /* Target 0, LUN 0 in virtio-scsi LUN addressing. */
    req->lun[0] = 1;
    req->lun[1] = 0;
    req->lun[2] = 0x40;
    req->lun[3] = 0;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    /* READ(10) of one 512-byte block at LBA 0. */
    req->cdb[0] = 0x28;
    req->cdb[8] = 0x01;

    resp->response = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    uint64_t resp_phys = vv_virt_to_phys(resp);

    /*
     * Data-in buffer points at the device's own common configuration
     * in PCI BAR MMIO space, not guest RAM. A completing READ that
     * DMAs the block into this range overwrites device_status and can
     * drive a reset from the completion path (compare T0063).
     */
    uint64_t bar_addr = dev->common_phys;

    vring_raw_set_desc(vr, 0, req_phys, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, bar_addr, 512,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* The completing READ DMAs into the device's own common config,
     * zeroing device_status. Unlike virtio-blk (T0063), virtio-scsi
     * does not reset from the completion path under the BQL, so the
     * host does not deadlock. Reaching this assertion proves the host
     * stayed alive. */
    vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);

    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0001, VIRTIO_PCI_DEVICE_SCSI, test_scsi_desc_addr_pci_bar,
                "Data-in descriptor pointing into PCI BAR MMIO region",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

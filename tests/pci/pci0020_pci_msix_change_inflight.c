/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0020: pci_msix_vector_change_inflight
 *
 * Change the MSI-X vector assignment for a queue while the queue
 * has an in-flight request. Tests device handling of interrupt
 * routing changes during active I/O processing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_msix_change_inflight(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    /* Kick to start I/O */
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Immediately change MSI-X vector for this queue */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    cfg->queue_select = vr->queue;
    __sync_synchronize();
    /* Try setting vector to 1 (different from initial 0) */
    cfg->queue_msix_vector = 1;
    __sync_synchronize();
    /* Change back */
    cfg->queue_msix_vector = 0;
    __sync_synchronize();

    /* Wait for completion */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != before)
            return TEST_PASS;
        elapsed += 10000;
    }

    __sync_synchronize();
    uint8_t st = cfg->device_status;
    if (st == 0)
        TWEDGED("st == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(PCI0020, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_change_inflight,
              "MSI-X vector changed while I/O in-flight",
              VIRTIO_SPEC_V1_2, "4.1.4.3");

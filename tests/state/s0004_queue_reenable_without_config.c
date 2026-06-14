/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0004: queue_reenable_without_config
 *
 * After full initialization, reset the queue (write queue_enable=0, wait
 * for readback), then immediately write queue_enable=1 without re-writing
 * the descriptor/avail/used addresses. The spec says the driver MUST
 * configure the queue resources during queue discovery/reset (4.1.4.3.2).
 *
 * A VMM that doesn't clear queue addresses on queue disable may retain
 * stale pointers. If it does clear them, enabling without config should
 * fail or be ignored.
 *
 * Note: VIRTIO_F_RING_RESET is needed for proper queue reset. We
 * attempt the operation regardless and observe behavior.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_queue_reenable_without_config(struct virtio_dev *dev,
                                                        struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Device is in DRIVER_OK with queue 0 enabled.
     * Try to disable the queue by writing enable=0.
     * (Spec says MUST NOT write 0, but we test what happens if the
     * VMM processes it anyway - the interesting part is the re-enable.)
     */
    cfg->queue_select = 0;
    __sync_synchronize();

    /* Save current addresses */
    uint64_t saved_desc = cfg->queue_desc;
    (void)saved_desc;

    /* Disable queue */
    cfg->queue_enable = 0;
    __sync_synchronize();
    usleep(50000);

    /*
     * Now re-enable WITHOUT writing desc/avail/used addresses.
     * If the VMM cleared them, this queue has null pointers.
     */
    cfg->queue_enable = 1;
    __sync_synchronize();
    usleep(10000);

    /* Attempt I/O through the "re-enabled" queue */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0004, VIRTIO_PCI_DEVICE_BLK, test_queue_reenable_without_config,
              "Re-enable queue without re-writing addresses after disable",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");

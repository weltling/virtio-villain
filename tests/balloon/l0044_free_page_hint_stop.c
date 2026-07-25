/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0044: free page hint stop command id.
 *
 * Spec 5.5.6.3: on the free page hint virtqueue the driver echoes the
 * device supplied command id, and to signal it has stopped reporting
 * it submits the reserved VIRTIO_BALLOON_CMD_ID_STOP value. l0035
 * exercises an arbitrary non reserved id and explicitly avoids STOP
 * and DONE; this submits the reserved STOP id as the command id buffer
 * and verifies the device consumes it and stays healthy.
 * Skips when the device does not offer free page hinting.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_balloon_free_page_hint_stop(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_BALLOON_F_FREE_PAGE_HINT)))
        return TEST_SKIP;

    /* Queue 2 is the free page hint VQ. */
    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring fpq;
    vring_alloc(&fpq, 16);
    vring_attach(dev, &fpq, 2);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Submit the reserved STOP command id as the 4 byte command id
     * buffer. This is the driver signal that it has stopped hinting. */
    uint32_t *cmd_id = vv_alloc_pages(1);
    *cmd_id = VIRTIO_BALLOON_CMD_ID_STOP;

    vring_raw_set_desc(&fpq, 0, vv_virt_to_phys(cmd_id), 4, 0, 0);
    vring_raw_set_avail(&fpq, 0, 0);
    vring_raw_set_avail_idx(&fpq, 1);

    return vv_kick_and_wait(dev, &fpq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST_REQUIRES(L0044, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_free_page_hint_stop,
              "Free page hint accepts the reserved STOP command id",
              VIRTIO_SPEC_V1_2, "5.5.6.3",
              (1ULL << VIRTIO_BALLOON_F_FREE_PAGE_HINT), 0);

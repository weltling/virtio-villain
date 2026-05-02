/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0010: split_to_packed_reinit
 *
 * Complete a full initialization with split virtqueue layout, submit
 * and complete a request, then reset the device and re-initialize
 * requesting VIRTIO_F_RING_PACKED for the same queue index.
 *
 * This exercises whether the VMM properly tears down split queue state
 * and can transition the same queue index to packed layout. A VMM with
 * stale split queue state (desc/avail/used pointers, size calculations)
 * may crash or misinterpret the packed descriptors.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_split_to_packed_reinit(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Phase 1: Do a valid I/O on the split queue (already set up by harness).
     */
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

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return TEST_SKIP; /* need working split first */

    /*
     * Phase 2: Reset and re-init with packed vring.
     */
    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    /* Check if device offers RING_PACKED */
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t feat_hi = cfg->device_feature;
    if (!(feat_hi & (1u << (VIRTIO_F_RING_PACKED - 32)))) {
        /* Device doesn't support packed - skip */
        return TEST_SKIP;
    }

    /* Negotiate only RING_PACKED */
    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = (1u << (VIRTIO_F_RING_PACKED - 32));
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    /* Set up packed vring on queue 0 (same index as previous split) */
    struct vring_packed pvr;
    vring_packed_alloc(&pvr, 16);
    vring_packed_attach(dev, &pvr, 0);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /*
     * Phase 3: Submit a request via packed layout on the same queue.
     */
    struct virtio_blk_outhdr *hdr2 = vv_alloc_pages(1);
    uint8_t *data2 = vv_alloc_pages(1);
    uint8_t *status2 = vv_alloc_pages(1);

    hdr2->type = VIRTIO_BLK_T_IN;
    hdr2->ioprio = 0;
    hdr2->sector = 0;
    *status2 = 0xFF;

    /* Descriptor 0: header (readable) */
    vring_packed_set_desc(&pvr, 0, vv_virt_to_phys(hdr2), sizeof(*hdr2),
                          0, VRING_PACKED_DESC_F_NEXT);
    /* Descriptor 1: data (writable) */
    vring_packed_set_desc(&pvr, 1, vv_virt_to_phys(data2), 512,
                          1, VRING_PACKED_DESC_F_NEXT | VRING_PACKED_DESC_F_WRITE);
    /* Descriptor 2: status (writable) */
    vring_packed_set_desc(&pvr, 2, vv_virt_to_phys(status2), 1,
                          2, VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, &pvr, 0, 2, pvr.wrap_counter, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0010, VIRTIO_PCI_DEVICE_BLK, test_split_to_packed_reinit,
              "Init with split vring then reset and switch to packed",
              VIRTIO_SPEC_V1_2, "3.1.1");

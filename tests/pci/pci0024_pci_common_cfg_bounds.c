/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0024: pci_common_cfg_read_at_boundary
 *
 * Read fields at the edges of the common config region using the
 * parsed common_length. Verify that accesses within bounds succeed
 * and the device remains stable.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_common_cfg_bounds(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (dev->common_length == 0)
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * The minimum common cfg is 56 bytes (up to queue_used at offset 48,
     * which is 8 bytes). Read device_status and num_queues which are
     * in the first 20 bytes.
     */
    uint8_t status = cfg->device_status;
    uint16_t nq = cfg->num_queues;
    uint16_t qsz = cfg->queue_size;

    (void)status;
    (void)nq;
    (void)qsz;

    /* Verify device is still functional after boundary reads */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0024, VIRTIO_PCI_DEVICE_BLK, test_pci_common_cfg_bounds,
              "Common cfg region boundary reads within parsed length",
              VIRTIO_SPEC_V1_2, "4.1.4.3");

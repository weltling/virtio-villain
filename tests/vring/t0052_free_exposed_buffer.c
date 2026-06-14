/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0052: free_exposed_buffer
 *
 * Submit a request, then immediately zero out the descriptor table
 * and the data buffers while the device may still be processing.
 * Simulates a driver freeing buffers before the device returns them
 * in the used ring.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_free_exposed_buffer(struct virtio_dev *dev,
                                              struct vring *vr)
{
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

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    virtio_pci_kick(dev, 0);

    /*
     * Immediately "free" the buffers by zeroing the descriptor table
     * and the backing memory. The device may be mid-DMA.
     */
    memset(hdr, 0, sizeof(*hdr));
    memset(data, 0, 4096);
    memset(status, 0, 1);
    vring_raw_set_desc(vr, 0, 0, 0, 0, 0);
    vring_raw_set_desc(vr, 1, 0, 0, 0, 0);
    vring_raw_set_desc(vr, 2, 0, 0, 0, 0);
    __sync_synchronize();

    usleep(500000);

    /* Survival = pass */
    return TEST_PASS;
}

REGISTER_TEST(T0052, VIRTIO_PCI_DEVICE_BLK, test_free_exposed_buffer,
              "Free exposed buffers while queue is live",
              VIRTIO_SPEC_V1_2, "3.3.1");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0075: desc_unmap_after_submit
 *
 * Submit a valid descriptor, then immediately zero out the data
 * buffer's backing memory (simulating an unmap/free while DMA is
 * in-flight). Tests device resilience to early buffer reclamation.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_desc_unmap_after_submit(struct virtio_dev *dev,
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

    /* Kick, then immediately poison the data buffer */
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);
    memset(data, 0xDE, 4096);  /* simulate unmap / overwrite */
    __sync_synchronize();

    /* Wait for completion */
    uint16_t before = vr->used->idx;
    int elapsed = 0;
    int step = 10000;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(step);
        __sync_synchronize();
        if (vr->used->idx != before)
            return TEST_PASS;
        elapsed += step;
    }
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    uint8_t st = cfg->device_status;
    if (st == 0)
        TWEDGED("st == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0075, VIRTIO_PCI_DEVICE_BLK, test_desc_unmap_after_submit,
              "Data buffer zeroed immediately after kick (unmap race)",
              VIRTIO_SPEC_V1_2, "2.7.5");

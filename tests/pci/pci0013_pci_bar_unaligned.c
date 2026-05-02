/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0013: pci_bar_unaligned_access
 *
 * Access the common config capability at odd byte offsets to test
 * unaligned register access handling. Some VMMs may assume all
 * accesses are naturally aligned and crash on unaligned reads.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static test_result_t test_pci_bar_unaligned(struct virtio_dev *dev,
                                            struct vring *vr)
{
    volatile uint8_t *base = (volatile uint8_t *)dev->common;

    /*
     * Read at odd offsets within the common config structure.
     * The structure starts at offset 0; device_feature_select is at +0,
     * device_feature at +4, etc. Read at offset 1, 3, 5 to create
     * unaligned accesses.
     */
    volatile uint8_t dummy = 0;
    dummy += base[1];
    dummy += base[3];
    dummy += base[5];
    dummy += base[7];
    (void)dummy;
    __sync_synchronize();

    usleep(10000);

    /* Verify device is still operational after unaligned accesses */
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

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0013, VIRTIO_PCI_DEVICE_BLK, test_pci_bar_unaligned,
              "Unaligned byte access to common config BAR",
              VIRTIO_SPEC_V1_2, "4.1.4.3");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0028: blk_write_partial_oob
 *
 * Submit a WRITE request where sector + (data_len / 512) exceeds the
 * device's capacity by exactly 1 sector. Unlike B01 which places sector
 * far beyond capacity, this is a boundary case where most of the write
 * is valid but the last sector overflows.
 *
 * Spec 5.2.6.1: driver MUST NOT submit a request which would cause a
 * read or write beyond capacity.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_write_partial_oob(struct virtio_dev *dev,
                                                struct vring *vr)
{
    /* Read device capacity from config space */
    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return TEST_SKIP;

    uint8_t cap_ptr = pci_cfg_read8(fd, 0x34);
    uint32_t dev_cfg_offset = 0;

    while (cap_ptr) {
        uint8_t cap_id = pci_cfg_read8(fd, cap_ptr);
        uint8_t cap_next = pci_cfg_read8(fd, cap_ptr + 1);
        if (cap_id == 0x09) {
            uint8_t cfg_type = pci_cfg_read8(fd, cap_ptr + 3);
            if (cfg_type == VIRTIO_PCI_CAP_DEVICE_CFG) {
                dev_cfg_offset = pci_cfg_read32(fd, cap_ptr + 8);
                break;
            }
        }
        cap_ptr = cap_next;
    }
    close(fd);

    if (!dev_cfg_offset)
        return TEST_SKIP;

    volatile uint64_t *cap_field = (volatile uint64_t *)
        ((char *)dev->bar + dev_cfg_offset);
    uint64_t capacity = *cap_field;

    if (capacity < 2)
        return TEST_SKIP;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /*
     * Write 2 sectors starting at (capacity - 1).
     * Sector (capacity - 1) is valid, sector (capacity) is OOB.
     */
    hdr->type = VIRTIO_BLK_T_OUT;
    hdr->ioprio = 0;
    hdr->sector = capacity - 1;
    *status = 0xFF;
    memset(data, 0xAA, 1024);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 1024, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0028, VIRTIO_PCI_DEVICE_BLK, test_blk_write_partial_oob,
              "WRITE spanning capacity boundary (partial OOB)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");

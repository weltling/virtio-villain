/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0024: device_status_reserved_bits_set
 *
 * Write reserved bits (bits 5-6 of device_status, excluding
 * NEEDS_RESET=64 and FAILED=128) to the device status register.
 * The device should ignore unknown bits or reject the write.
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

static test_result_t test_status_reserved_bits(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Get current status (should have DRIVER_OK etc.) */
    uint8_t orig = cfg->device_status;

    /*
     * Bit 5 (0x20) is reserved in virtio 1.2. Write it along with
     * the current valid bits. The device should either ignore the
     * reserved bit or reject the write entirely.
     */
    cfg->device_status = orig | 0x20;
    __sync_synchronize();
    usleep(10000);

    uint8_t after = cfg->device_status;

    /* The device should either:
     * 1) Ignore the reserved bit (status == orig), or
     * 2) Keep the reserved bit (status == orig | 0x20), or
     * 3) Set NEEDS_RESET/FAILED if it considers this fatal
     * Any of these is acceptable; a crash is not.
     */
    if (after == 0)
        TWEDGED("after == 0");

    /* Verify device is still functional */
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

REGISTER_TEST(S0024, VIRTIO_PCI_DEVICE_BLK, test_status_reserved_bits,
              "Device status write with reserved bit 5 set",
              VIRTIO_SPEC_V1_2, "2.1");

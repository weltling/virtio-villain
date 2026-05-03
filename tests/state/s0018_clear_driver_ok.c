/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0018: clear_driver_ok
 *
 * Set DRIVER_OK then immediately clear it by writing device_status
 * without the DRIVER_OK bit. The spec says drivers MUST NOT clear
 * feature bits after DRIVER_OK, and clearing status bits is
 * generally not supported (only full reset via writing 0).
 * This tests whether the device handles partial status regression.
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

static test_result_t test_clear_driver_ok(struct virtio_dev *dev,
                                          struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Device is already in DRIVER_OK state (init.c set it).
     * Clear the DRIVER_OK bit while keeping other bits.
     */
    uint8_t current = cfg->device_status;
    cfg->device_status = current & ~VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
    usleep(10000);

    /*
     * Now try to kick a request. The device should either:
     * - Ignore the write (spec says "not valid to clear bits")
     * - Treat device as not ready and reject the kick
     * - Set NEEDS_RESET
     */
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

REGISTER_TEST(S0018, VIRTIO_PCI_DEVICE_BLK, test_clear_driver_ok,
              "Clear DRIVER_OK bit without full reset",
              VIRTIO_SPEC_V1_2, "3.1.1");

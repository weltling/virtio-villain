/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0022: notif_suppress_never_consume
 *
 * Enable notification suppression (set AVAIL_NO_INTERRUPT flag) then
 * submit many requests without ever consuming used entries. Tests
 * device behavior under backpressure when the used ring fills up
 * but the driver never advances the used_event/last_used.
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

static test_result_t test_notif_suppress_backpressure(struct virtio_dev *dev,
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

    /* Set VRING_AVAIL_F_NO_INTERRUPT to suppress notifications */
    vr->avail->flags = 1; /* VRING_AVAIL_F_NO_INTERRUPT */
    __sync_synchronize();

    /* Submit 8 requests using desc slots 0-2 repeatedly (3 per request) */
    uint16_t avail_idx = 0;
    for (int req = 0; req < 8; req++) {
        uint16_t base = (req * 3) % (vr->size - 3);
        vring_raw_set_desc(vr, base, hdr_phys, sizeof(*hdr),
                           VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, data_phys, 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
        vring_raw_set_desc(vr, base + 2, status_phys, 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, avail_idx, base);
        avail_idx++;
        vring_raw_set_avail_idx(vr, avail_idx);
        __sync_synchronize();
        virtio_pci_kick(dev, vr->queue);
        /* Do NOT consume used entries */
    }

    /* Wait a bit then check if device is alive */
    usleep(VV_TIMEOUT_MS * 1000);
    __sync_synchronize();

    /* Check used ring - device should have completed at least some */
    if (vr->used->idx > 0)
        return TEST_PASS;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    uint8_t st = cfg->device_status;
    if (st == 0)
        TWEDGED("st == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(S0022, VIRTIO_PCI_DEVICE_BLK, test_notif_suppress_backpressure,
              "Notification suppression with used ring never consumed",
              VIRTIO_SPEC_V1_2, "2.7.7");

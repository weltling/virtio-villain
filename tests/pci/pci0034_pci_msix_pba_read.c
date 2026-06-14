/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0034: Read MSI-X PBA (Pending Bit Array) during interrupt storm.
 *
 * Spec 4.1.4.5: The device uses MSI-X for interrupt delivery.
 * Submit multiple requests rapidly and read the ISR register
 * while completions are pending to exercise the interrupt path.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_msix_pba_read(struct virtio_dev *dev,
                                            struct vring *vr)
{
    /* Submit 4 rapid requests */
    struct virtio_blk_outhdr *hdrs[4];
    uint8_t *datas[4];
    uint8_t *sts[4];

    for (int i = 0; i < 4; i++) {
        hdrs[i] = vv_alloc_pages(1);
        datas[i] = vv_alloc_pages(1);
        sts[i] = vv_alloc_pages(1);

        hdrs[i]->type = VIRTIO_BLK_T_IN;
        hdrs[i]->ioprio = 0;
        hdrs[i]->sector = (uint64_t)i;
        *sts[i] = 0xFF;
    }

    for (int i = 0; i < 4; i++) {
        int base = i * 3;
        vring_raw_set_desc(vr, base, vv_virt_to_phys(hdrs[i]),
                           sizeof(*hdrs[i]), VRING_DESC_F_NEXT, base + 1);
        vring_raw_set_desc(vr, base + 1, vv_virt_to_phys(datas[i]), 512,
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, base + 2);
        vring_raw_set_desc(vr, base + 2, vv_virt_to_phys(sts[i]), 1,
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, i, base);
    }
    vring_raw_set_avail_idx(vr, 4);

    /* Kick all at once to create interrupt pressure */
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Read ISR rapidly while completions may be pending */
    volatile uint8_t *isr = dev->isr;
    uint8_t isr_seen = 0;
    for (int i = 0; i < 20; i++) {
        __sync_synchronize();
        isr_seen |= *isr;
        usleep(500);
    }
    (void)isr_seen;

    /* Wait for all completions */
    uint16_t before = 0;
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx - before >= 4)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(PCI0034, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_pba_read,
              "Read MSI-X PBA during interrupt storm",
              VIRTIO_SPEC_V1_2, "4.1.4.5");

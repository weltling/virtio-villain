/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0019: pci_config_read_during_io
 *
 * Read device configuration space (config generation + capacity) while
 * an I/O request is in-flight. Tests concurrent access to config vs
 * data path without device internal locking issues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_config_during_io(struct virtio_dev *dev,
                                               struct vring *vr)
{
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

    /* Kick to start I/O */
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Immediately read config while I/O is in-flight */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    for (int i = 0; i < 100; i++) {
        (void)cfg->config_generation;
        __sync_synchronize();
        (void)cfg->device_status;
        __sync_synchronize();
        (void)cfg->num_queues;
        __sync_synchronize();
    }

    /* Now wait for I/O completion */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx != before)
            return TEST_PASS;
        elapsed += 10000;
    }

    __sync_synchronize();
    uint8_t st = cfg->device_status;
    if (st == 0)
        TWEDGED("st == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(PCI0019, VIRTIO_PCI_DEVICE_BLK, test_pci_config_during_io,
              "Config space reads concurrent with in-flight I/O",
              VIRTIO_SPEC_V1_2, "4.1.4.3");

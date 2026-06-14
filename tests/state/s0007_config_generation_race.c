/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0007: config_generation_race
 *
 * Read device configuration fields while continuously polling
 * config_generation. This simulates a driver that reads multi-field
 * config across a generation change without retrying - which the
 * spec warns about (4.1.4.3.2).
 *
 * We can't force the device to change config, but we can perform
 * rapid interleaved reads of generation + config to stress the path.
 * We also attempt a reset mid-read to trigger a generation bump.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_config_generation_race(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Find device config offset */
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

    volatile uint8_t *dev_cfg = (volatile uint8_t *)dev->bar + dev_cfg_offset;

    /*
     * Read config generation, then read capacity (8 bytes), then check
     * generation again. Do this in a tight loop. For the malicious
     * variant, trigger a device reset mid-read to force a gen bump.
     */
    uint8_t gen_before, gen_after;
    uint64_t capacity;

    /* Normal racing reads */
    for (int i = 0; i < 100; i++) {
        gen_before = cfg->config_generation;
        capacity = *(volatile uint64_t *)dev_cfg;
        gen_after = cfg->config_generation;
        (void)capacity;
        (void)gen_before;
        (void)gen_after;
    }

    /*
     * Now the interesting part: reset the device between reading the
     * first half and second half of a multi-word config field.
     */
    gen_before = cfg->config_generation;
    uint32_t cap_lo = *(volatile uint32_t *)dev_cfg;

    /* Reset mid-read! */
    cfg->device_status = 0;
    __sync_synchronize();

    uint32_t cap_hi = *(volatile uint32_t *)(dev_cfg + 4);
    gen_after = cfg->config_generation;

    (void)cap_lo;
    (void)cap_hi;
    (void)gen_before;
    (void)gen_after;

    usleep(50000);

    /*
     * Re-initialize and verify device is alive.
     */
    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_DRIVER;
    __sync_synchronize();

    cfg->driver_feature_select = 0;
    cfg->driver_feature = 0;
    cfg->driver_feature_select = 1;
    cfg->driver_feature = 0;
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();

    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        return TEST_SKIP;

    struct vring vr2;
    vring_alloc(&vr2, 16);
    vring_attach(dev, &vr2, 0);
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /* Verify with a simple read */
    struct {
        uint32_t type;
        uint32_t ioprio;
        uint64_t sector;
    } __attribute__((packed)) *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), 16,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(S0007, VIRTIO_PCI_DEVICE_BLK, test_config_generation_race,
              "Read config across device reset to race generation counter",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");

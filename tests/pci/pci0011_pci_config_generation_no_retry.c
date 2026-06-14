/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0011: pci_config_generation_no_retry
 *
 * Read device config across a generation change without retrying.
 * The spec says the driver must re-read config if config_generation
 * changes between reads. Here we deliberately split a multi-word read
 * and verify the device doesn't crash if we use a potentially torn value.
 *
 * We trigger a config generation change by resetting mid-read.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_config_gen_no_retry(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    (void)vr;

    /* Find device config */
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

    volatile uint32_t *cap_lo = (volatile uint32_t *)
        ((char *)dev->bar + dev_cfg_offset);
    volatile uint32_t *cap_hi = (volatile uint32_t *)
        ((char *)dev->bar + dev_cfg_offset + 4);

    /* Read generation, read low word of capacity */
    uint8_t gen1 = cfg->config_generation;
    uint32_t lo = *cap_lo;
    (void)gen1;
    (void)lo;

    /* Force a generation change by resetting */
    virtio_pci_reset(dev);

    /* Read high word WITHOUT checking generation (violation) */
    uint32_t hi = *cap_hi;
    (void)hi;

    /* Use the torn capacity value to construct a request */
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

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(&vr2, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr2, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr2, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&vr2, 0, 0);
    vring_raw_set_avail_idx(&vr2, 1);

    return vv_kick_and_wait(dev, &vr2, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0011, VIRTIO_PCI_DEVICE_BLK, test_pci_config_gen_no_retry,
              "Read config across generation change without retry",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");

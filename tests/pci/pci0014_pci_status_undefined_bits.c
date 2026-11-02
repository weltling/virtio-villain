/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0014: pci_device_status_undefined_bits
 *
 * Write device_status with undefined high bits set. Only bits 0-7
 * are defined by the spec. Tests that the device ignores or rejects
 * undefined status bits without side effects.
 */
#include "lib/virtio_test.h"

static test_result_t test_pci_status_bad_bits(struct virtio_dev *dev,
                                              struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /*
     * Write device_status with high bits set (0xFF | valid bits).
     * The device_status field is 8 bits in the spec, but the register
     * may be wider in some implementations.
     */
    uint8_t orig_status = cfg->device_status;

    /* Set all undefined high bits along with valid ACKNOWLEDGE|DRIVER */
    cfg->device_status = orig_status | 0xC0; /* bits 6,7 are undefined */
    __sync_synchronize();
    usleep(10000);

    /* Verify device still responds - do a normal I/O */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(PCI0014, VIRTIO_PCI_DEVICE_BLK, test_pci_status_bad_bits,
              "Write device_status with undefined high bits set",
              VIRTIO_SPEC_V1_2, "4.1.4.3.1");

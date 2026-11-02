/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0040: change_features_after_ok
 *
 * Attempt to re-write driver feature bits after FEATURES_OK has been
 * set. The spec says features are frozen after FEATURES_OK.
 */
#include "lib/virtio_test.h"

static test_result_t test_change_features_after_ok(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    /*
     * Device is in DRIVER_OK state (harness already initialized).
     * Now try to change driver features.
     */
    dev->common->driver_feature_select = 0;
    dev->common->driver_feature = 0xFFFFFFFF;
    dev->common->driver_feature_select = 1;
    dev->common->driver_feature = 0xFFFFFFFF;
    __sync_synchronize();
    usleep(10000);

    /* Do a normal I/O to verify device still works */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    vv_desc(vr, 0, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vv_desc(vr, 2, status, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(T0040, VIRTIO_PCI_DEVICE_BLK, test_change_features_after_ok,
              "Change driver features after FEATURES_OK",
              VIRTIO_SPEC_V1_2, "2.2");

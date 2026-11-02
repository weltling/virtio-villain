/* SPDX-License-Identifier: Apache-2.0 */
/*
 * S0001: double_feature_negotiation
 *
 * Complete feature negotiation with FEATURES_OK, then without a full
 * device reset, clear only driver_features and attempt to re-negotiate
 * a different feature set. The spec says the driver MUST NOT accept new
 * feature bits after FEATURES_OK (3.1.1).
 *
 * A VMM that allows feature renegotiation without reset may enter an
 * inconsistent internal state where it believes different features are
 * active than what was originally agreed upon.
 */
#include "lib/virtio_test.h"

static test_result_t test_double_feature_negotiation(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    /*
     * Device is in DRIVER_OK with base transport features negotiated.
     * Now attempt to re-negotiate features without resetting.
     */

    /* Write new feature bits (all 1s) - this violates the freeze */
    dev->common->driver_feature_select = 0;
    dev->common->driver_feature = 0xFFFFFFFF;
    dev->common->driver_feature_select = 1;
    dev->common->driver_feature = 0xFFFFFFFF;
    __sync_synchronize();

    /* Re-assert FEATURES_OK to try to "confirm" the new features */
    dev->common->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(10000);

    /* Now try to do I/O - the device state may be confused */
    struct virtio_blk_outhdr *hdr = blk_hdr(VIRTIO_BLK_T_IN, 0);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, data, 512, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1, VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(S0001, VIRTIO_PCI_DEVICE_BLK, test_double_feature_negotiation,
              "Re-negotiate features without reset after FEATURES_OK",
              VIRTIO_SPEC_V1_2, "3.1.1");

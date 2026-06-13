/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0022: balloon_reporting_vq_no_feature
 *
 * Probe whether the device exposes a fourth queue without
 * advertising VIRTIO_BALLOON_F_REPORTING. Spec 5.5.5 ties
 * the page reporting queue to the feature. If the queue is
 * absent the test passes trivially; if present the device must
 * accept a kick on it without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_balloon_reporting_no_feature(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (feat & (1U << VIRTIO_BALLOON_F_REPORTING))
        return TEST_SKIP;

    /*
     * Without page reporting feature the reporting queue must
     * not exist. Probe queue index 3 and confirm queue_size
     * reads zero.
     */
    cfg->queue_select = 3;
    __sync_synchronize();
    if (cfg->queue_size != 0)
        TFAIL("cfg->queue_size != 0");

    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST(L0022, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_reporting_no_feature,
              "Reporting vq absent without PAGE_REPORTING",
              VIRTIO_SPEC_V1_2, "5.5.5");

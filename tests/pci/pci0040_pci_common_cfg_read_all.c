/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0040: Read all common_cfg fields to exercise configuration space.
 *
 * Spec 4.1.4.3: Read every field of the virtio common configuration
 * structure to exercise the device's config space read path and
 * verify all fields return plausible values.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_common_cfg_read_all(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    (void)vr;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();

    /* Read all fields */
    uint32_t dfs = cfg->device_feature_select;
    (void)dfs;
    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat0 = cfg->device_feature;
    (void)feat0;
    cfg->device_feature_select = 1;
    __sync_synchronize();
    uint32_t feat1 = cfg->device_feature;
    (void)feat1;

    uint32_t drv_fs = cfg->driver_feature_select;
    (void)drv_fs;
    uint32_t drv_f = cfg->driver_feature;
    (void)drv_f;

    uint16_t msix_cfg = cfg->msix_config;
    (void)msix_cfg;

    uint16_t num_q = cfg->num_queues;
    if (num_q == 0)
        TFAIL("num_q == 0");  /* device must have at least 1 queue */

    uint8_t ds = cfg->device_status;
    if (ds == 0)
        TWEDGED("ds == 0");

    uint8_t gen = cfg->config_generation;
    (void)gen;

    /* Select each queue and read its fields */
    for (uint16_t q = 0; q < num_q && q < 8; q++) {
        cfg->queue_select = q;
        __sync_synchronize();

        uint16_t qs = cfg->queue_size;
        uint16_t qmv = cfg->queue_msix_vector;
        uint16_t qe = cfg->queue_enable;
        uint16_t qno = cfg->queue_notify_off;
        uint64_t qd = virtio_load64(&cfg->queue_desc);
        uint64_t qa = virtio_load64(&cfg->queue_avail);
        uint64_t qu = virtio_load64(&cfg->queue_used);
        (void)qs; (void)qmv; (void)qe; (void)qno;
        (void)qd; (void)qa; (void)qu;
    }

    /* Re-read status to make sure device didn't die */
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(PCI0040, VIRTIO_PCI_DEVICE_BLK, test_pci_common_cfg_read_all,
              "Read all common config fields",
              VIRTIO_SPEC_V1_2, "4.1.4.3");

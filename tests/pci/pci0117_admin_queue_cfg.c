/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0117: admin virtqueue common-config fields are consistent.
 *
 * Spec 4.1.4.3: when VIRTIO_F_ADMIN_VQ is negotiated the PCI common
 * configuration exposes admin_queue_index and admin_queue_num after
 * queue_reset. num_queues excludes administration virtqueues, so the
 * admin queues occupy indices admin_queue_index .. +admin_queue_num.
 * Verify admin_queue_num is at least one and the admin range starts
 * at or after the regular queues.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_pci_admin_queue_cfg(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;

    if (!virtio_pci_feature_offered(dev, VIRTIO_F_ADMIN_VQ))
        return TEST_SKIP;

    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    uint16_t num_queues = cfg->num_queues;
    uint16_t admin_index = cfg->admin_queue_index;
    uint16_t admin_num = cfg->admin_queue_num;

    if (admin_num == 0)
        TFAIL("admin_queue_num is 0 despite ADMIN_VQ offered");
    if (admin_index < num_queues)
        TFAIL("admin_queue_index %u overlaps the %u regular queues",
              admin_index, num_queues);

    return TEST_PASS;
}

REGISTER_TEST(PCI0117, VIRTIO_PCI_DEVICE_BLK, test_pci_admin_queue_cfg,
              "Admin virtqueue common-config fields are consistent",
              VIRTIO_SPEC_V1_4, "4.1.4.3");

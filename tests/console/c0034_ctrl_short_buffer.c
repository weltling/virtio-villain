/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0034: Control message with truncated buffer on ctrl VQ.
 *
 * Spec 5.3.6.1: A control message is 8 bytes. Send only 2 bytes.
 * The device must detect the short descriptor and not read beyond.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_ctrl_short(struct virtio_dev *dev,
                                             struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    uint32_t feat = cfg->device_feature;
    if (!(feat & (1u << VIRTIO_CONSOLE_F_MULTIPORT)))
        return TEST_SKIP;

    cfg->queue_select = 2;
    __sync_synchronize();
    if (cfg->queue_size == 0)
        return TEST_SKIP;

    struct vring cq;
    vring_alloc(&cq, 16);
    vring_attach(dev, &cq, 2);

    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 8);

    /* Only 2 bytes, way shorter than 8 byte control struct */
    vring_raw_set_desc(&cq, 0, vv_virt_to_phys(buf), 2, 0, 0);
    vring_raw_set_avail(&cq, 0, 0);
    vring_raw_set_avail_idx(&cq, 1);

    return vv_kick_and_wait(dev, &cq, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0034, VIRTIO_PCI_DEVICE_CONSOLE,
              test_console_ctrl_short,
              "Truncated control message on ctrl VQ",
              VIRTIO_SPEC_V1_2, "5.3.6.1");

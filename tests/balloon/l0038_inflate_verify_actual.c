/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0038: inflate one page then verify actual config updates.
 *
 * Spec 5.5.6.1: After the driver inflates pages, it writes the
 * new actual count to the config actual field. Inflate one page,
 * write actual=1, then read it back and verify the device accepted
 * the value.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_balloon_inflate_actual(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!dev->device_cfg || dev->device_cfg_length < 8)
        return TEST_SKIP;

    volatile struct virtio_balloon_config_head *bcfg =
        (volatile struct virtio_balloon_config_head *)dev->device_cfg;

    /* Inflate one page (PFN of a known page) */
    uint32_t *pfn_buf = vv_alloc_pages(1);
    void *page = vv_alloc_pages(1);
    pfn_buf[0] = (uint32_t)(vv_virt_to_phys(page) >> VIRTIO_BALLOON_PFN_SHIFT);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(pfn_buf), 4, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS) return r;

    /* Write actual = 1 (we inflated one page) */
    bcfg->actual = 1;
    __sync_synchronize();
    usleep(10000);

    /* Read it back */
    uint32_t readback = bcfg->actual;
    if (readback != 1)
        TFAIL("actual readback %u, expected 1", readback);

    return TEST_PASS;
}

REGISTER_TEST(L0038, VIRTIO_PCI_DEVICE_BALLOON, test_balloon_inflate_actual,
              "Inflate one page then verify actual config updates",
              VIRTIO_SPEC_V1_2, "5.5.6.1");

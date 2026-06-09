/* SPDX-License-Identifier: Apache-2.0 */
/*
 * D0030: repeated pings reset the watchdog deadline.
 *
 * Cloud Hypervisor virtio-watchdog (Device ID 35) has no spec
 * chapter. Each accepted ping must reset the timeout. Submit
 * three pings spaced a few seconds apart. All three must
 * complete and the device must not register a timeout in
 * between.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t one_ping(struct virtio_dev *dev, struct vring *vr,
                              uint8_t *resp, uint16_t slot)
{
    *resp = 0;
    vring_raw_set_desc(vr, slot, vv_virt_to_phys(resp), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, slot, slot);
    vring_raw_set_avail_idx(vr, (uint16_t)(slot + 1));
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

static test_result_t test_watchdog_ping_resets_timer(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint8_t *resp = vv_alloc_pages(1);
    test_result_t r;

    r = one_ping(dev, vr, resp, 0);
    if (r != TEST_PASS) return r;
    sleep(3);

    r = one_ping(dev, vr, resp, 1);
    if (r != TEST_PASS) return r;
    sleep(3);

    r = one_ping(dev, vr, resp, 2);
    return r;
}

REGISTER_TEST(D0030, VIRTIO_PCI_DEVICE_WATCHDOG,
              test_watchdog_ping_resets_timer,
              "Spaced pings keep the watchdog timer alive",
              VIRTIO_SPEC_V1_4, "-");

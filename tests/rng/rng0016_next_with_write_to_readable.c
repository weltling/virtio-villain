/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0016: rng_next_with_write_to_readable
 *
 * Submit a chain whose first descriptor is writable plus NEXT and
 * the second descriptor is readable. Spec 2.7.5.2 says all device
 * writable descriptors must follow all device readable ones in a
 * chain. The device must reject or stay silent.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_next_write_then_read(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    uint8_t *out = vv_alloc_pages(1);
    uint8_t *in = vv_alloc_pages(1);
    memset(in, 0, 16);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(out), 64,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(in), 16,
                       0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0016, VIRTIO_PCI_DEVICE_RNG, test_rng_next_write_then_read,
              "RNG chain with writable then readable order",
              VIRTIO_SPEC_V1_2, "2.7.5.2");

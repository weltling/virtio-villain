/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0019: rng_desc_spans_two_pages
 *
 * Submit an entropy request whose buffer straddles a page boundary.
 * Spec 2.7.5.1 says descriptors describe a contiguous physical
 * region. A cross page buffer is valid but exercises the device
 * scatter path. The device must fill the buffer without corrupting
 * adjacent memory.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_cross_page(struct virtio_dev *dev,
                                         struct vring *vr)
{
    uint8_t *pages = vv_alloc_pages(2);
    /* Start 32 bytes before the page boundary */
    uint8_t *buf = pages + 4096 - 32;
    memset(buf, 0, 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 64,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0019, VIRTIO_PCI_DEVICE_RNG, test_rng_cross_page,
              "Entropy buffer spanning a page boundary",
              VIRTIO_SPEC_V1_2, "2.7.5.1");

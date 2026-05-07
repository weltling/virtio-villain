/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0002: RNG zero-length writable descriptor.
 *
 * Submit a writable requestq descriptor of length 0. The device
 * MAY use less than the entire buffer length per spec 5.4.6.2,
 * but must not write past the (non-existent) buffer or crash.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_zero_len(struct virtio_dev *dev,
                                       struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 0, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0002, VIRTIO_PCI_DEVICE_RNG, test_rng_zero_len,
              "RNG zero-length writable descriptor",
              VIRTIO_SPEC_V1_2, "5.4.6");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * RNG0008: RNG multi-segment writable chain.
 *
 * Submit a 3-segment writable chain on the requestq. This is the
 * legitimate way a driver provides scatter-gather random buffers.
 * The device must walk the chain and fill bytes across the
 * segments without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_rng_chain_writable(struct virtio_dev *dev,
                                             struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 32,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, buf_phys + 32, 32,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, buf_phys + 64, 32,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(RNG0008, VIRTIO_PCI_DEVICE_RNG, test_rng_chain_writable,
              "RNG multi-segment writable chain",
              VIRTIO_SPEC_V1_2, "5.4.6");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0006: Pmem flush short request.
 *
 * Request descriptor of 1 byte, smaller than uint32_t type.
 * Device must reject the malformed input.
 *
 * Spec 5.10.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_pmem_resp { uint32_t ret; } __attribute__((packed));

static test_result_t test_pmem_short_req(struct virtio_dev *dev,
                                         struct vring *vr)
{
    void *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 1,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 64,
                       sizeof(struct virtio_pmem_resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0006, VIRTIO_PCI_DEVICE_PMEM, test_pmem_short_req,
              "Flush short request",
              VIRTIO_SPEC_V1_2, "5.10.6.1");

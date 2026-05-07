/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0011: Virtio-mem short request.
 *
 * Submit a request descriptor of only 2 bytes, smaller than
 * struct virtio_mem_req. Device must reject the malformed input.
 *
 * Spec 5.14.6.2.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>

struct virtio_mem_resp { uint16_t type; uint16_t p[3]; uint16_t state; }
    __attribute__((packed));

static test_result_t test_mem_short_req(struct virtio_dev *dev,
                                        struct vring *vr)
{
    void *buf = vv_alloc_pages(1);
    memset(buf, 0, 64);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 2,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(buf) + 64,
                       sizeof(struct virtio_mem_resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0011, VIRTIO_PCI_DEVICE_MEM, test_mem_short_req,
              "Short request descriptor",
              VIRTIO_SPEC_V1_2, "5.14.6.2");

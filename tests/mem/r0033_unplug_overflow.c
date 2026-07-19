/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0033: virtio-mem UNPLUG with addr + length overflowing u64.
 *
 * Spec 5.15.6.2: A range must lie within the usable region. An
 * UNPLUG whose addr plus nb_blocks*block_size overflows a 64
 * bit value can confuse a device that bounds checks the end
 * address by computing it without overflow detection. Submit
 * UNPLUG with addr near the top of the address space and a
 * large nb_blocks. The device must reject the request rather
 * than walking off the end of its plugged range bitmap.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_unplug_overflow(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->type      = VIRTIO_MEM_REQ_UNPLUG;
    req->addr      = 0xFFFFFFFFFFFFF000ULL;
    req->nb_blocks = 0xFFFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0033, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_overflow,
              "UNPLUG with addr plus length overflowing u64",
              VIRTIO_SPEC_V1_2, "5.15.6.2");

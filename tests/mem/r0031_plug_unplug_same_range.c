/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0031: virtio_mem PLUG then UNPLUG of the same range in one batch.
 *
 * Spec 5.14.6.2: PLUG and UNPLUG operate on a contiguous block
 * range. Submit a PLUG followed by an UNPLUG of the same range
 * in the same avail batch and single kick. The device must
 * serialise the two requests and leave the range fully unplugged,
 * without leaking state or wedging on the back to back ops.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_plug_unplug_same_range(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    uint8_t *p = vv_alloc_pages(2);
    memset(p, 0, 8192);

    struct virtio_mem_req  *r1 = (void *)p;
    struct virtio_mem_resp_wide *s1 = (void *)(p + 64);
    struct virtio_mem_req  *r2 = (void *)(p + 128);
    struct virtio_mem_resp_wide *s2 = (void *)(p + 192);

    r1->type = VIRTIO_MEM_REQ_PLUG;
    r1->addr = 0;
    r1->nb_blocks = 1;
    r2->type = VIRTIO_MEM_REQ_UNPLUG;
    r2->addr = 0;
    r2->nb_blocks = 1;

    uint64_t base = vv_virt_to_phys(p);

    vring_raw_set_desc(vr, 0, base,             sizeof(*r1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + 64,        sizeof(*s1),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 2, base + 128,       sizeof(*r2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, base + 192,       sizeof(*s2),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    return vv_kick_and_wait_n(dev, vr, 0, 2, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0031, VIRTIO_PCI_DEVICE_MEM,
              test_mem_plug_unplug_same_range,
              "PLUG then UNPLUG of the same range in one batch",
              VIRTIO_SPEC_V1_2, "5.14.6.2");

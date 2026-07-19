/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0017: mem_plug_overflow
 *
 * Submit a PLUG request whose addr plus nb_blocks times block_size
 * overflows a 64 bit value. Spec 5.15.6 requires bounds checking
 * before the device acts. The device must reject without integer
 * wraparound and stay alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_plug_overflow(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xFF, sizeof(*resp));

    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = 0xFFFFFFFFFFFFF000ULL;
    req->nb_blocks = 0xFFFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0017, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_overflow,
              "Plug with addr plus length overflowing u64",
              VIRTIO_SPEC_V1_2, "5.15.6");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0032: virtio-mem UNPLUG with addr unaligned to block_size.
 *
 * Spec 5.14.6.2: addr must be a multiple of block_size for both
 * PLUG and UNPLUG. Submit an UNPLUG with addr offset by one
 * byte. The device must reject the request rather than
 * truncating addr to a valid alignment and unplugging a
 * different physical range.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_mem_unplug_unaligned(struct virtio_dev *dev,
                                               struct vring *vr)
{
    struct virtio_mem_req  *req  = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));

    req->type      = VIRTIO_MEM_REQ_UNPLUG;
    req->addr      = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16) + 1;
    req->nb_blocks = 1;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0032, VIRTIO_PCI_DEVICE_MEM, test_mem_unplug_unaligned,
              "UNPLUG with addr unaligned to block_size",
              VIRTIO_SPEC_V1_2, "5.14.6.2");

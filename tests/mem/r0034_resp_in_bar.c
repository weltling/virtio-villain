/* SPDX-License-Identifier: Apache-2.0 */
/*
 * R0034: Virtio-mem response descriptor aimed at the device MMIO BAR.
 *
 * Spec 5.15.6.2: The mem response is a writable buffer.
 * Submit a PLUG whose writable response descriptor points at
 * the device's own common configuration BAR rather than RAM.
 * A device that writes the response through the generic memory
 * API without validating the target region can corrupt its own
 * registers or wedge. The device must reject or handle the non
 * RAM target cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_mem_resp_in_bar(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_mem_req *req = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = *(volatile uint64_t *)((uint8_t *)dev->device_cfg + 16);
    req->nb_blocks = 1;

    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, mmio_phys, sizeof(struct virtio_mem_resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(R0034, VIRTIO_PCI_DEVICE_MEM, test_mem_resp_in_bar,
              "Mem response descriptor points at device MMIO BAR",
              VIRTIO_SPEC_V1_2, "5.15.6.2");

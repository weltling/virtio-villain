/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0022: Pmem flush with truncated response descriptor.
 *
 * Spec 5.10.6.1: The response is a 4 byte virtio_pmem_resp. Submit
 * a flush whose writable response descriptor is only 2 bytes long.
 * The device must not write past the descriptor length and must
 * still complete or cleanly reject the request.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pmem_flush_truncated_resp(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);

    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    memset(resp, 0xFF, 4);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), 2,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0022, VIRTIO_PCI_DEVICE_PMEM, test_pmem_flush_truncated_resp,
              "Pmem flush response descriptor truncated to 2 bytes",
              VIRTIO_SPEC_V1_2, "5.19.6.1");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0015: Pmem flush reserved request bytes.
 *
 * Allocate a 32-byte request region with bytes 4..31 set to 0xAA.
 * Only the first 4 bytes are defined; the device must ignore any
 * trailing data and still process the flush.
 *
 * Spec 5.10.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_pmem_reserved_bytes(struct virtio_dev *dev,
                                              struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0xAA, 256);
    *(uint32_t *)buf = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 32,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0015, VIRTIO_PCI_DEVICE_PMEM, test_pmem_reserved_bytes,
              "Flush oversized request",
              VIRTIO_SPEC_V1_2, "5.10.6.1");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * V0069: vsock OP_RW header descriptor in device MMIO BAR.
 *
 * Spec 5.10.6: A vsock packet starts with a readable header
 * descriptor on the tx queue. Submit an OP_RW whose readable
 * header descriptor addr points at the device's own common
 * configuration BAR rather than RAM. A device that reads the
 * header through the generic memory API without validating
 * the source region can interpret register bytes as src_cid,
 * dst_cid, len, op, and dispatch on garbage values. The
 * device must reject or handle the non RAM source cleanly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

static test_result_t test_vsock_hdr_in_bar(struct virtio_dev *dev,
                                           struct vring *vr)
{
    uint8_t *payload = vv_alloc_pages(1);
    uint64_t mmio_phys = dev->common_phys;

    vring_raw_set_desc(vr, 0, mmio_phys, 44, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(payload), 16, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(V0069, VIRTIO_PCI_DEVICE_VSOCK, test_vsock_hdr_in_bar,
              "vsock OP_RW header descriptor in device MMIO BAR",
              VIRTIO_SPEC_V1_2, "5.10.6");

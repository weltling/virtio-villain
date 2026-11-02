/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0026: Pmem flush response descriptor aimed at the device MMIO BAR.
 *
 * Spec 5.19.6.1: The pmem flush response is a writable buffer.
 * Submit a flush whose writable response descriptor points at
 * the device's own common configuration BAR rather than RAM.
 * A device that performs the response write through the generic
 * memory API without validating the target region can wedge or
 * corrupt its own registers. The device must reject or handle
 * the non RAM target cleanly.
 */
#include "lib/virtio_test.h"

static test_result_t test_pmem_resp_in_bar(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_pmem_req *req = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;

    uint64_t mmio_phys = dev->common_phys;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, mmio_phys, sizeof(struct virtio_pmem_resp),
                       VRING_DESC_F_WRITE, 0);
    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST(E0026, VIRTIO_PCI_DEVICE_PMEM, test_pmem_resp_in_bar,
              "Flush response descriptor points at device MMIO BAR",
              VIRTIO_SPEC_V1_2, "5.19.6.1");

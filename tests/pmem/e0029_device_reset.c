/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0029: pmem FLUSH after device reset and reinit.
 *
 * Spec 3.1 plus 5.19.6: After a reset the device must be ready
 * for fresh requests. Reset, renegotiate zero features, set
 * up the queue, and issue a single FLUSH.
 */
#include "tests/test.h"
#include "lib/virtio_spec.h"
#include "lib/util.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_pmem_reset_flush(struct virtio_dev *dev,
                                           struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    virtio_pci_reset(dev);

    cfg->device_status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;
    __sync_synchronize();
    cfg->driver_feature_select = 0; cfg->driver_feature = 0;
    cfg->driver_feature_select = 1; cfg->driver_feature = 0;
    __sync_synchronize();
    cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
    __sync_synchronize();
    usleep(1000);
    if (!(cfg->device_status & VIRTIO_STATUS_FEATURES_OK))
        TREJECT("FEATURES_OK rejected after reset");
    cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    struct vring tvr;
    if (vring_alloc(&tvr, 16) < 0) return TEST_SKIP;
    vring_attach(dev, &tvr, 0);

    struct virtio_pmem_req  *req  = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp = vv_alloc_pages(1);
    req->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp->ret = 0xFFFFFFFFu;

    vring_raw_set_desc(&tvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&tvr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&tvr, 0, 0);
    vring_raw_set_avail_idx(&tvr, 1);

    return vv_kick_and_wait(dev, &tvr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(E0029, VIRTIO_PCI_DEVICE_PMEM, test_pmem_reset_flush,
              "pmem FLUSH immediately after device reset",
              VIRTIO_SPEC_V1_4, "3.1");

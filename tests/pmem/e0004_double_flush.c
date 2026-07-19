/* SPDX-License-Identifier: Apache-2.0 */
/*
 * E0004: Pmem double flush in rapid succession.
 *
 * Submit two flush requests back-to-back to exercise concurrent
 * request handling in the pmem device.
 *
 * Spec 5.19.6.1: Stress test.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pmem_double_flush(struct virtio_dev *dev,
                                            struct vring *vr)
{
    struct virtio_pmem_req *req1 = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp1 = vv_alloc_pages(1);
    struct virtio_pmem_req *req2 = vv_alloc_pages(1);
    struct virtio_pmem_resp *resp2 = vv_alloc_pages(1);

    req1->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    req2->type = VIRTIO_PMEM_REQ_TYPE_FLUSH;
    resp1->ret = 0xFF;
    resp2->ret = 0xFF;

    /* First request: descs 0-1 */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req1), sizeof(*req1),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp1), sizeof(*resp1),
                       VRING_DESC_F_WRITE, 0);

    /* Second request: descs 2-3 */
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(req2), sizeof(*req2),
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(resp2), sizeof(*resp2),
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    /* Kick and wait for both completions */
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if (vr->used->idx >= 2)
            return TEST_PASS;
        elapsed += 10000;
    }

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(E0004, VIRTIO_PCI_DEVICE_PMEM, test_pmem_double_flush,
              "Two flush requests in rapid succession",
              VIRTIO_SPEC_V1_2, "5.19.6.1");

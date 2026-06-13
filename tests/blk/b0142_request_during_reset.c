/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0142: Request during device reset.
 *
 * Submit a request and immediately trigger a device reset by writing
 * 0 to device_status. Tests race between request processing and reset.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_request_during_reset(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(vr, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, data_phys, 512,
                       VRING_DESC_F_WRITE | VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    __sync_synchronize();

    /* Kick and immediately reset */
    virtio_pci_kick(dev, vr->queue);
    cfg->device_status = 0;
    __sync_synchronize();

    usleep(100000);

    /*
     * After reset, device_status should read 0. The device must not
     * crash regardless of whether the request was partially processed.
     */
    if (cfg->device_status != 0)
        TFAIL("cfg->device_status != 0");

    return TEST_PASS;
}

REGISTER_TEST(B0142, VIRTIO_PCI_DEVICE_BLK, test_blk_request_during_reset,
              "Request during device reset",
              VIRTIO_SPEC_V1_2, "5.2.6");

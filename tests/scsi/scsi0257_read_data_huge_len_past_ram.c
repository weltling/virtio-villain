/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0257: scsi read with data-in len past end of guest RAM.
 *
 * Same shape as B0162 for the virtio-scsi request path: submit a
 * READ(10) at LBA 0 whose data-in descriptor base lives in valid guest
 * RAM but whose length crosses the end of all System RAM. The device
 * must not access memory outside the guest's mapping or crash the VMM.
 *
 * Spec 5.6.6.1.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_scsi_read_huge_len_past_ram(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    /* Target 0, LUN 0 in virtio-scsi LUN addressing. */
    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    /* READ(10) of one 512-byte block at LBA 0. */
    req->cdb[0] = 0x28;
    req->cdb[8] = 0x01;

    resp->response = 0xFF;

    uint64_t req_phys = vv_virt_to_phys(req);
    uint64_t resp_phys = vv_virt_to_phys(resp);

    uint64_t data_phys;
    if (!vv_alloc_page_near_ram_top(ram_top, &data_phys))
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - data_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vring_raw_set_desc(vr, 0, req_phys, sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, resp_phys, sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, data_phys, len,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(SCSI0257, VIRTIO_PCI_DEVICE_SCSI,
                test_scsi_read_huge_len_past_ram,
                "Scsi read data-in len crosses end of RAM",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

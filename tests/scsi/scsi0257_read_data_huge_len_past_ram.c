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
#include "lib/virtio_test.h"

static test_result_t test_scsi_read_huge_len_past_ram(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    /* Target 0, LUN 0 in virtio-scsi LUN addressing. */
    req->lun[0] = 1;
    req->lun[2] = 0x40;
    req->task_attr = VIRTIO_SCSI_S_SIMPLE;
    /* READ(10) of one 512-byte block at LBA 0. */
    req->cdb[0] = 0x28;
    req->cdb[8] = 0x01;

    resp->response = 0xFF;

    uint64_t data_phys;
    if (!vv_alloc_page_near_ram_top(ram_top, &data_phys))
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - data_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_NEXT | VRING_DESC_F_WRITE,
            2);
    vring_raw_set_desc(vr, 2, data_phys, len,
                       VRING_DESC_F_WRITE, 0);

    return vv_submit_and_wait(dev, vr, 0);
}

REGISTER_TEST_Q(SCSI0257, VIRTIO_PCI_DEVICE_SCSI,
                test_scsi_read_huge_len_past_ram,
                "Scsi read data-in len crosses end of RAM",
                VIRTIO_SPEC_V1_2, "5.6.6.1", VV_QUEUE_LAST);

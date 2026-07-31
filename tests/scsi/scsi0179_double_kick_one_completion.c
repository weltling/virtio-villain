/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0179: double_kick_one_completion
 *
 * Kicking the queue twice for a single posted request yields exactly
 * one completion, not a duplicate.
 */
#include "tests/scsi/scsi_util.h"

#include <unistd.h>

static test_result_t test_scsi_double_kick(struct virtio_dev *dev,
                                           struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);
    uint16_t before = vr->used->idx;

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x28;      /* READ(10) */
    req->cdb[8] = 1;
    resp->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);

    virtio_pci_kick(dev, vr->queue);
    virtio_pci_kick(dev, vr->queue);
    usleep(100000);

    __sync_synchronize();
    if ((uint16_t)(vr->used->idx - before) != 1)
        TFAIL("used advanced by %u, expected 1",
              (uint16_t)(vr->used->idx - before));
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0179, VIRTIO_PCI_DEVICE_SCSI, test_scsi_double_kick,
                "Double kick of one request yields one completion",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

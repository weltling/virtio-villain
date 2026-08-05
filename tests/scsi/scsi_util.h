/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Shared helpers for the virtio-scsi test family. Keeps LUN encoding
 * and request submission out of every test body.
 */
#ifndef VV_SCSI_UTIL_H
#define VV_SCSI_UTIL_H

#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

/* Fill an 8-byte virtio-scsi LUN for the given target and logical unit. */
static inline void scsi_set_lun(uint8_t *lun, uint8_t target, uint16_t l)
{
    memset(lun, 0, 8);
    lun[0] = 1;
    lun[1] = target;
    lun[2] = 0x40 | ((l >> 8) & 0x3f);
    lun[3] = l & 0xff;
}

/* Read a big-endian 32-bit field out of a SCSI data buffer. */
static inline uint32_t scsi_be32(const volatile uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

/*
 * Submit one SCSI command on the request queue and wait for the
 * completion. data may be NULL for a command with no transfer. dir
 * selects the data direction: SCSI_DATA_NONE, SCSI_DATA_IN (device
 * writes into data) or SCSI_DATA_OUT (device reads from data). The
 * caller passes a monotonically increasing seq so several commands can
 * reuse the same ring.
 */
#define SCSI_DATA_NONE 0
#define SCSI_DATA_IN   1
#define SCSI_DATA_OUT  2

static inline test_result_t scsi_do_cmd(struct virtio_dev *dev,
                                        struct vring *vr,
                                        struct virtio_scsi_cmd_req *req,
                                        struct virtio_scsi_cmd_resp *resp,
                                        void *data, uint32_t datalen,
                                        int dir, uint16_t seq)
{
    resp->response = 0xFF;
    resp->status = 0xFF;

    if (dir == SCSI_DATA_OUT && data) {
        /* req readable, data-out readable, resp writable. */
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), datalen,
                           VRING_DESC_F_NEXT, 2);
        vring_raw_set_desc(vr, 2, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
    } else if (dir == SCSI_DATA_IN && data) {
        /* req readable, resp writable, data-in writable. */
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), datalen,
                           VRING_DESC_F_WRITE, 0);
    } else {
        /* req readable, resp writable. */
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
    }

    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

/*
 * Issue one throwaway command to clear the power-on UNIT ATTENTION a
 * target reports after reset. Returns the next seq to use.
 */
static inline uint16_t scsi_clear_ua(struct virtio_dev *dev, struct vring *vr,
                                     uint16_t seq)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x00;      /* TEST UNIT READY */
    scsi_do_cmd(dev, vr, req, resp, NULL, 0, SCSI_DATA_NONE, seq);
    return seq + 1;
}

/*
 * Submit one control queue task management function request and wait
 * for the completion.
 */
static inline test_result_t scsi_do_tmf(struct virtio_dev *dev,
                                        struct vring *vr,
                                        struct virtio_scsi_ctrl_tmf_req *req,
                                        struct virtio_scsi_ctrl_tmf_resp *resp,
                                        uint16_t seq)
{
    resp->response = 0xAA;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

/* True if the byte is one of the defined control queue response codes. */
static inline int scsi_tmf_response_valid(uint8_t response)
{
    return response == VIRTIO_SCSI_S_FUNCTION_COMPLETE ||
           response == VIRTIO_SCSI_S_FUNCTION_SUCCEEDED ||
           response == VIRTIO_SCSI_S_FUNCTION_REJECTED ||
           response == VIRTIO_SCSI_S_INCORRECT_LUN ||
           response == VIRTIO_SCSI_S_BAD_TARGET;
}

/* True for a defined SCSI completion status: GOOD or CHECK CONDITION. */
static inline int scsi_status_defined(uint8_t status)
{
    return status == 0x00 || status == 0x02;
}

/*
 * Submit one command on the request queue without waiting for the
 * completion, so the caller can leave it in flight and act on it from
 * another queue. Returns the used ring index captured just before the
 * kick, which scsi_wait_used polls against. dir selects the data
 * direction as in scsi_do_cmd.
 */
static inline uint16_t scsi_submit_async(struct virtio_dev *dev,
                                         struct vring *vr,
                                         struct virtio_scsi_cmd_req *req,
                                         struct virtio_scsi_cmd_resp *resp,
                                         void *data, uint32_t datalen,
                                         int dir, uint16_t seq)
{
    resp->response = 0xFF;
    resp->status = 0xFF;

    if (dir == SCSI_DATA_OUT && data) {
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), datalen,
                           VRING_DESC_F_NEXT, 2);
        vring_raw_set_desc(vr, 2, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
    } else if (dir == SCSI_DATA_IN && data) {
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vring_raw_set_desc(vr, 2, vv_virt_to_phys(data), datalen,
                           VRING_DESC_F_WRITE, 0);
    } else {
        vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
    }

    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);
    return before;
}

/*
 * Poll the used ring until it advances past *before* or the timeout
 * in microseconds elapses. Returns 1 if it advanced, 0 on timeout.
 */
static inline int scsi_wait_used(struct vring *vr, uint16_t before,
                                 int timeout_us)
{
    int elapsed = 0;
    while (elapsed < timeout_us) {
        usleep(20000);
        __sync_synchronize();
        if (vr->used->idx != before)
            return 1;
        elapsed += 20000;
    }
    return 0;
}

/*
 * Submit a command whose data buffer is described one page per
 * descriptor. Buffers from vv_alloc_pages are virtually contiguous but
 * physically scattered, so a single descriptor spanning more than a
 * page would DMA later bytes to the wrong physical page. Splitting per
 * page maps each page to its own physical frame, which data integrity
 * checks across a page boundary require.
 */
static inline test_result_t scsi_do_cmd_paged(struct virtio_dev *dev,
                                              struct vring *vr,
                                              struct virtio_scsi_cmd_req *req,
                                              struct virtio_scsi_cmd_resp *resp,
                                              uint8_t *data, uint32_t len,
                                              int dir, uint16_t seq)
{
    resp->response = 0xFF;
    resp->status = 0xFF;

    uint32_t pages = (len + 4095) / 4096;
    uint16_t d = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    d = 1;

    if (dir == SCSI_DATA_OUT) {
        for (uint32_t i = 0; i < pages; i++) {
            uint32_t chunk = len - i * 4096 < 4096 ? len - i * 4096 : 4096;
            vring_raw_set_desc(vr, d, vv_virt_to_phys(data + i * 4096),
                               chunk, VRING_DESC_F_NEXT, d + 1);
            d++;
        }
        vring_raw_set_desc(vr, d, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_WRITE, 0);
    } else {
        vring_raw_set_desc(vr, d, vv_virt_to_phys(resp), sizeof(*resp),
                           VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, d + 1);
        d++;
        for (uint32_t i = 0; i < pages; i++) {
            uint32_t chunk = len - i * 4096 < 4096 ? len - i * 4096 : 4096;
            uint16_t last = i == pages - 1;
            vring_raw_set_desc(vr, d, vv_virt_to_phys(data + i * 4096),
                               chunk,
                               VRING_DESC_F_WRITE |
                                   (last ? 0 : VRING_DESC_F_NEXT),
                               last ? 0 : d + 1);
            d++;
        }
    }

    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

#endif /* VV_SCSI_UTIL_H */

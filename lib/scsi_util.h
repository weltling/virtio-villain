/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Shared helpers for the virtio-scsi test family. Keeps LUN encoding
 * and request submission out of every test body.
 */
#ifndef VV_SCSI_UTIL_H
#define VV_SCSI_UTIL_H

#include "virtio_test.h"
#include "mmio_helpers.h"

#include <time.h>

static inline void scsi_kick(struct virtio_dev *dev, uint16_t queue)
{
    if (dev->mmio)
        virtio_mmio_kick(dev->mmio, queue);
    else
        virtio_pci_kick(dev, queue);
}

static inline int scsi_feature_offered(struct virtio_dev *dev, unsigned bit)
{
    if (!dev->mmio)
        return virtio_pci_feature_offered(dev, bit);
    mmio_write32(dev->mmio, VIRTIO_MMIO_DEVICE_FEATURES_SEL, bit / 32);
    return !!(mmio_read32(dev->mmio, VIRTIO_MMIO_DEVICE_FEATURES)
              & (1U << (bit % 32)));
}

static inline test_result_t scsi_kick_and_wait(struct virtio_dev *dev,
                                               struct vring *ring)
{
    if (!dev->mmio)
        return vv_kick_and_wait(dev, ring, 0, VV_TIMEOUT_MS);
    uint16_t before = ring->used->idx;
    scsi_kick(dev, ring->queue);
    for (int elapsed = 0; elapsed < VV_TIMEOUT_MS * vv_wait_scale; elapsed++) {
        __sync_synchronize();
        if (ring->used->idx != before)
            return TEST_PASS;
        usleep(1000);
    }
    TWEDGED("MMIO SCSI queue did not complete");
}

static inline test_result_t scsi_run_mmio(struct virtio_mmio_dev *mmio,
                                         test_fn fn, uint16_t queue,
                                         uint64_t features)
{
    struct virtio_dev dev = { .mmio = mmio };
    struct vring ring;
    if (virtio_mmio_init_features(mmio, features) < 0)
        TFAIL("MMIO SCSI feature negotiation failed");
    if (vring_alloc(&ring, 16) < 0)
        TFAIL("MMIO SCSI queue allocation failed");
    VV_TRY(mmio_bind_queue(mmio, &ring, queue));
    mmio_write32(mmio, VIRTIO_MMIO_STATUS,
                 mmio_read32(mmio, VIRTIO_MMIO_STATUS)
                 | VIRTIO_MMIO_STATUS_DRIVER_OK);
    return fn(&dev, &ring);
}

#define REGISTER_SCSI_MMIO(tname, func, queue, features) \
    static test_result_t mmio_##tname(struct virtio_mmio_dev *dev) \
    { \
        return scsi_run_mmio(dev, func, queue, features); \
    } \
    REGISTER_MMIO_VARIANT(tname, 8, mmio_##tname)

/* Reset before replacing queues that the harness already enabled. */
static inline void scsi_attach_request_queue(struct virtio_dev *dev,
                                              struct vring *control,
                                              struct vring *request,
                                              uint16_t queue)
{
    if (dev->mmio) {
        if (virtio_mmio_init(dev->mmio) < 0)
            vv_die("MMIO SCSI device reset failed");
        if (mmio_bind_queue(dev->mmio, control, 0) != TEST_PASS
            || mmio_bind_queue(dev->mmio, request, queue) != TEST_PASS)
            vv_die("MMIO SCSI queue attach failed");
        mmio_write32(dev->mmio, VIRTIO_MMIO_STATUS,
                     mmio_read32(dev->mmio, VIRTIO_MMIO_STATUS)
                     | VIRTIO_MMIO_STATUS_DRIVER_OK);
        return;
    }
    if (virtio_pci_init(dev) < 0)
        vv_die("SCSI device reset failed");
    vring_attach(dev, control, 0);
    vring_attach(dev, request, queue);
    dev->common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();
}

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
        vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
        vv_desc(vr, 1, data, datalen, VRING_DESC_F_NEXT, 2);
        vv_desc(vr, 2, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    } else if (dir == SCSI_DATA_IN && data) {
        /* req readable, resp writable, data-in writable. */
        vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
        vv_desc(vr, 1, resp, sizeof(*resp),
                VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vv_desc(vr, 2, data, datalen, VRING_DESC_F_WRITE, 0);
    } else {
        /* req readable, resp writable. */
        vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
        vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    }

    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    return scsi_kick_and_wait(dev, vr);
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
    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    return scsi_kick_and_wait(dev, vr);
}

/* A kick on another queue does not guarantee that the request is pending. */
static inline test_result_t scsi_tmf_when_pending(
    struct virtio_dev *dev, struct vring *control,
    const struct virtio_scsi_cmd_req *request,
    const volatile struct virtio_scsi_cmd_resp *request_response,
    struct virtio_scsi_ctrl_tmf_req *tmf,
    struct virtio_scsi_ctrl_tmf_resp *tmf_response)
{
    struct virtio_scsi_ctrl_tmf_req *query = vv_alloc_pages(1);
    struct virtio_scsi_ctrl_tmf_resp *response = vv_alloc_pages(1);
    query->type = VIRTIO_SCSI_T_TMF;
    query->subtype = VIRTIO_SCSI_T_TMF_QUERY_TASK;
    memcpy(query->lun, request->lun, sizeof(query->lun));
    query->tag = request->tag;
    struct timespec start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
        vv_die("SCSI task synchronization clock failed");
    for (;;) {
        __sync_synchronize();
        if (request_response->response != 0xFF)
            TSKIP("request completed before the task management operation");
        test_result_t result = scsi_do_tmf(
            dev, control, query, response, control->avail->idx);
        if (result != TEST_PASS)
            return result;
        if (response->response == VIRTIO_SCSI_S_FUNCTION_SUCCEEDED)
            return scsi_do_tmf(dev, control, tmf, tmf_response,
                               control->avail->idx);
        if (response->response != VIRTIO_SCSI_S_FUNCTION_COMPLETE)
            TFAIL("QUERY TASK synchronization response 0x%02x",
                  response->response);
        if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
            vv_die("SCSI task synchronization clock failed");
        int64_t elapsed = (int64_t)(now.tv_sec - start.tv_sec) * 1000000000
                        + now.tv_nsec - start.tv_nsec;
        if (elapsed >= 2000000000)
            TFAIL("submitted request never became observable to QUERY TASK");
        usleep(1000);
    }
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
        vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
        vv_desc(vr, 1, data, datalen, VRING_DESC_F_NEXT, 2);
        vv_desc(vr, 2, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    } else if (dir == SCSI_DATA_IN && data) {
        vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
        vv_desc(vr, 1, resp, sizeof(*resp),
                VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
        vv_desc(vr, 2, data, datalen, VRING_DESC_F_WRITE, 0);
    } else {
        vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
        vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    }

    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    uint16_t before = vr->used->idx;
    __sync_synchronize();
    scsi_kick(dev, vr->queue);
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

    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    d = 1;

    if (dir == SCSI_DATA_OUT) {
        for (uint32_t i = 0; i < pages; i++) {
            uint32_t chunk = len - i * 4096 < 4096 ? len - i * 4096 : 4096;
            vv_desc(vr, d, data + i * 4096, chunk, VRING_DESC_F_NEXT, d + 1);
            d++;
        }
        vv_desc(vr, d, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    } else {
        vv_desc(vr, d, resp, sizeof(*resp),
                VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, d + 1);
        d++;
        for (uint32_t i = 0; i < pages; i++) {
            uint32_t chunk = len - i * 4096 < 4096 ? len - i * 4096 : 4096;
            uint16_t last = i == pages - 1;
            vv_desc(vr, d, data + i * 4096, chunk,
                    VRING_DESC_F_WRITE | (last ? 0 : VRING_DESC_F_NEXT),
                    last ? 0 : d + 1);
            d++;
        }
    }

    vring_raw_set_avail(vr, seq, 0);
    vring_raw_set_avail_idx(vr, seq + 1);
    return scsi_kick_and_wait(dev, vr);
}

#endif /* VV_SCSI_UTIL_H */

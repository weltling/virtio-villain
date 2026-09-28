/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_SOUND_UTIL_H
#define VV_SOUND_UTIL_H

#include "tests/test.h"
#include "lib/util.h"
#include "lib/virtio_snd.h"

#include <stdint.h>
#include <string.h>

#define SND_JACK_INFO_SIZE   24
#define SND_PCM_INFO_SIZE    32
#define SND_CHMAP_INFO_SIZE  24

#define SND_PCM_FMT_S16    5
#define SND_PCM_RATE_48000 7

#define SND_PCM_FEATURES_VALID  ((1u << 5) - 1)
#define SND_PCM_FORMATS_VALID   ((1ULL << 26) - 1)
#define SND_PCM_RATES_VALID     ((1ULL << 14) - 1)

static inline test_result_t snd_submit_control(struct virtio_dev *dev,
                                               struct vring *vr,
                                               const void *request,
                                               uint32_t request_len,
                                               uint32_t response_len,
                                               uint32_t *status,
                                               uint32_t *used_len)
{
    uint8_t *req = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);
    uint16_t avail_idx = vr->avail->idx;
    uint16_t used_idx = vr->used->idx;

    if (request_len > PAGE_SIZE || response_len > PAGE_SIZE)
        TFAIL("sound control message exceeds one page");

    memcpy(req, request, request_len);
    memset(resp, 0xff, response_len);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), request_len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), response_len,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, avail_idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, avail_idx + 1);

    test_result_t result = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (result == TEST_PASS) {
        *used_len = vr->used->ring[used_idx % vr->size].len;
        *status = ((struct virtio_snd_hdr *)resp)->code;
    }
    return result;
}

static inline test_result_t snd_expect_safe_handling(struct virtio_dev *dev,
                                                     struct vring *vr,
                                                     const void *request,
                                                     uint32_t request_len,
                                                     uint32_t response_len)
{
    uint32_t status = 0;
    uint32_t used_len = 0;
    test_result_t result = snd_submit_control(
        dev, vr, request, request_len, response_len, &status, &used_len);
    if (result != TEST_PASS)
        return result;

    if (used_len < sizeof(struct virtio_snd_hdr))
        TFAIL("sound request completed with only %u response bytes", used_len);
    if (used_len > response_len)
        TFAIL("sound response length %u exceeds writable buffer %u",
              used_len, response_len);

    if (status == VIRTIO_SND_S_OK)
        return TEST_PASS;
    if (status != VIRTIO_SND_S_BAD_MSG &&
        status != VIRTIO_SND_S_NOT_SUPP &&
        status != VIRTIO_SND_S_IO_ERR)
        TFAIL("invalid sound response status 0x%08x", status);

    TREJECT("sound request safely rejected with status 0x%04x", status);
}

static inline test_result_t snd_expect_ok(struct virtio_dev *dev,
                                          struct vring *vr,
                                          const void *request,
                                          uint32_t request_len)
{
    uint32_t status = 0;
    uint32_t used_len = 0;
    test_result_t result = snd_submit_control(
        dev, vr, request, request_len, sizeof(struct virtio_snd_hdr),
        &status, &used_len);
    if (result != TEST_PASS)
        return result;
    if (used_len < sizeof(struct virtio_snd_hdr))
        TFAIL("sound request completed with %u response bytes", used_len);
    if (status != VIRTIO_SND_S_OK)
        TFAIL("valid sound request returned status 0x%04x", status);
    return TEST_PASS;
}

static inline volatile struct virtio_snd_config *
snd_config(struct virtio_dev *dev)
{
    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct virtio_snd_config))
        return NULL;
    return (volatile struct virtio_snd_config *)dev->device_cfg;
}

#endif /* VV_SOUND_UTIL_H */

/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_pcm_info_request_split(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->streams == 0)
        return TEST_SKIP;

    struct virtio_snd_query_info *req = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);
    const uint32_t response_len =
        sizeof(struct virtio_snd_hdr) + sizeof(struct virtio_snd_pcm_info);
    uint16_t used_idx = vr->used->idx;

    req->hdr.code = VIRTIO_SND_R_PCM_INFO;
    req->start_id = 0;
    req->count = 1;
    req->size = sizeof(struct virtio_snd_pcm_info);
    memset(resp, 0xff, response_len);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), 8,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys((uint8_t *)req + 8),
                       sizeof(*req) - 8, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(resp), response_len,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, vr->avail->idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, vr->avail->idx + 1);

    test_result_t result = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (result != TEST_PASS)
        return result;
    uint32_t used_len = vr->used->ring[used_idx % vr->size].len;
    if (used_len != response_len)
        TFAIL("split request used length %u, expected %u",
              used_len, response_len);
    if (((struct virtio_snd_hdr *)resp)->code != VIRTIO_SND_S_OK)
        TFAIL("split request status 0x%08x",
              ((struct virtio_snd_hdr *)resp)->code);
    return TEST_PASS;
}

REGISTER_TEST(SND0017, VIRTIO_PCI_DEVICE_SND,
              test_pcm_info_request_split,
              "PCM_INFO request split across readable descriptors",
              VIRTIO_SPEC_V1_2, "5.14.6.2");

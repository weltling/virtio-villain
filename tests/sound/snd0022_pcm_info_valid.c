/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_pcm_info_valid(struct virtio_dev *dev,
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

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp), response_len,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, vr->avail->idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, vr->avail->idx + 1);

    test_result_t result = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (result != TEST_PASS)
        return result;
    uint32_t used_len = vr->used->ring[used_idx % vr->size].len;
    if (used_len != response_len)
        TFAIL("PCM_INFO used length %u, expected %u",
              used_len, response_len);

    struct virtio_snd_hdr *hdr = (struct virtio_snd_hdr *)resp;
    struct virtio_snd_pcm_info *info =
        (struct virtio_snd_pcm_info *)(resp + sizeof(*hdr));
    if (hdr->code != VIRTIO_SND_S_OK)
        TFAIL("PCM_INFO status 0x%08x", hdr->code);
    if (info->formats == 0 || info->rates == 0)
        TFAIL("PCM_INFO advertises no formats or rates");
    if (info->features & ~SND_PCM_FEATURES_VALID)
        TFAIL("PCM_INFO features 0x%08x contain undefined bits",
              info->features);
    if (info->formats & ~SND_PCM_FORMATS_VALID)
        TFAIL("PCM_INFO formats 0x%016llx contain undefined bits",
              (unsigned long long)info->formats);
    if (info->rates & ~SND_PCM_RATES_VALID)
        TFAIL("PCM_INFO rates 0x%016llx contain undefined bits",
              (unsigned long long)info->rates);
    if (info->direction > 1)
        TFAIL("PCM_INFO direction %u is undefined", info->direction);
    if (info->channels_min == 0 ||
        info->channels_max < info->channels_min)
        TFAIL("PCM_INFO channel range %u..%u is invalid",
              info->channels_min, info->channels_max);
    for (size_t i = 0; i < sizeof(info->padding); i++) {
        if (info->padding[i] != 0)
            TFAIL("PCM_INFO padding byte %zu is 0x%02x",
                  i, info->padding[i]);
    }
    return TEST_PASS;
}

REGISTER_TEST(SND0022, VIRTIO_PCI_DEVICE_SND,
              test_pcm_info_valid,
              "Enumerate and validate one PCM stream",
              VIRTIO_SPEC_V1_2, "5.14.6.6.2");

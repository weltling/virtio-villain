/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

#define SND_CHMAP_POSITION_MAX  37

static test_result_t test_chmap_info_valid(struct virtio_dev *dev,
                                           struct vring *vr)
{
    volatile struct virtio_snd_config *cfg = snd_config(dev);
    if (!cfg || cfg->chmaps == 0)
        return TEST_SKIP;

    struct virtio_snd_query_info *req = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);
    const uint32_t response_len =
        sizeof(struct virtio_snd_hdr) + sizeof(struct virtio_snd_chmap_info);
    uint16_t used_idx = vr->used->idx;

    req->hdr.code = VIRTIO_SND_R_CHMAP_INFO;
    req->start_id = 0;
    req->count = 1;
    req->size = sizeof(struct virtio_snd_chmap_info);
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
        TFAIL("CHMAP_INFO used length %u, expected %u",
              used_len, response_len);

    struct virtio_snd_hdr *hdr = (struct virtio_snd_hdr *)resp;
    struct virtio_snd_chmap_info *info =
        (struct virtio_snd_chmap_info *)(resp + sizeof(*hdr));
    if (hdr->code != VIRTIO_SND_S_OK)
        TFAIL("CHMAP_INFO status 0x%08x", hdr->code);
    if (info->direction > 1)
        TFAIL("CHMAP_INFO direction %u is undefined", info->direction);
    if (info->channels > VIRTIO_SND_CHMAP_MAX_SIZE)
        TFAIL("CHMAP_INFO channel count %u exceeds %u",
              info->channels, VIRTIO_SND_CHMAP_MAX_SIZE);
    for (uint8_t i = 0; i < info->channels; i++) {
        if (info->positions[i] > SND_CHMAP_POSITION_MAX)
            TFAIL("CHMAP_INFO position %u is undefined",
                  info->positions[i]);
    }
    return TEST_PASS;
}

REGISTER_TEST(SND0027, VIRTIO_PCI_DEVICE_SND,
              test_chmap_info_valid,
              "Enumerate and validate one channel map",
              VIRTIO_SPEC_V1_2, "5.14.6.7.1");

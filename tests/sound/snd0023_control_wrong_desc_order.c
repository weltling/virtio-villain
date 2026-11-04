/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/sound_util.h"

static test_result_t test_control_wrong_desc_order(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_snd_query_info *req = vv_alloc_pages(1);
    uint8_t *resp = vv_alloc_pages(1);
    uint16_t used_idx = vr->used->idx;

    req->hdr.code = VIRTIO_SND_R_PCM_INFO;
    req->start_id = 0;
    req->count = 0;
    req->size = SND_PCM_INFO_SIZE;
    memset(resp, 0xff, sizeof(struct virtio_snd_hdr) + 1);

    vv_desc(vr, 0, resp, sizeof(struct virtio_snd_hdr),
            VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1);
    vv_desc(vr, 1, req, sizeof(*req), 0, 0);
    vring_raw_set_avail(vr, vr->avail->idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, vr->avail->idx + 1);

    VV_TRY(vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS));
    uint32_t used_len = vr->used->ring[used_idx % vr->size].len;
    if (used_len > sizeof(struct virtio_snd_hdr))
        TFAIL("wrong-order response length %u exceeds writable buffer",
              used_len);
    if (resp[sizeof(struct virtio_snd_hdr)] != 0xff)
        TFAIL("wrong-order control chain overran writable descriptor");
    return TEST_PASS;
}

REGISTER_TEST(SND0023, VIRTIO_PCI_DEVICE_SND,
              test_control_wrong_desc_order,
              "Control chain with writable before readable",
              VIRTIO_SPEC_V1_2, "2.7.5.2");

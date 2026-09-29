/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_info_request_addr_wrap(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    uint8_t *resp = vv_alloc_pages(1);
    memset(resp, 0xff, sizeof(struct virtio_snd_hdr));

    vring_raw_set_desc(vr, 0, 0xfffffffffffff000ULL, 0x2000,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(resp),
                       sizeof(struct virtio_snd_hdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, vr->avail->idx % vr->size, 0);
    vring_raw_set_avail_idx(vr, vr->avail->idx + 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(SND0025, VIRTIO_PCI_DEVICE_SND,
              test_info_request_addr_wrap,
              "PCM_INFO request address plus length wraps",
              VIRTIO_SPEC_V1_2, "2.7.5");

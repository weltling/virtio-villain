/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_chmap_info_range_overflow(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_snd_query_info req = {
        .hdr.code = VIRTIO_SND_R_CHMAP_INFO,
        .start_id = UINT32_MAX,
        .count = 2,
        .size = SND_CHMAP_INFO_SIZE,
    };
    return snd_expect_safe_handling(dev, vr, &req, sizeof(req),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0019, VIRTIO_PCI_DEVICE_SND,
              test_chmap_info_range_overflow,
              "Bound overflowing CHMAP_INFO item range",
              VIRTIO_SPEC_V1_2, "5.14.6.2");

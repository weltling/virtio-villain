/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_bad_control_messages_safe(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    struct virtio_snd_hdr truncated = {
        .code = VIRTIO_SND_R_PCM_START,
    };
    test_result_t result = snd_expect_safe_handling(
        dev, vr, &truncated, sizeof(truncated),
        sizeof(struct virtio_snd_hdr));
    if (result != TEST_PASS && result != TEST_REJECT)
        return result;

    struct virtio_snd_hdr unknown = {
        .code = 0xffffffff,
    };
    return snd_expect_safe_handling(dev, vr, &unknown, sizeof(unknown),
                                    sizeof(struct virtio_snd_hdr));
}

REGISTER_TEST(SND0009, VIRTIO_PCI_DEVICE_SND, test_bad_control_messages_safe,
              "Safely handle truncated and unknown controls",
              VIRTIO_SPEC_V1_2, "5.14.6");

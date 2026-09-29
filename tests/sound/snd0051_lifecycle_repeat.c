/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_lifecycle_repeat(struct virtio_dev *dev,
                                           struct vring *vr)
{
    test_result_t result = snd_complete_lifecycle(dev, vr, 0);
    if (result != TEST_PASS)
        return result;
    return snd_complete_lifecycle(dev, vr, 0);
}

REGISTER_TEST(SND0051, VIRTIO_PCI_DEVICE_SND,
              test_lifecycle_repeat,
              "Repeat a valid PCM lifecycle",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");

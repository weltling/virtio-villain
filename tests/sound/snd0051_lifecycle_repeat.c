/* SPDX-License-Identifier: Apache-2.0 */
#include "lib/sound_util.h"

static test_result_t test_lifecycle_repeat(struct virtio_dev *dev,
                                           struct vring *vr)
{
    VV_TRY(snd_complete_lifecycle(dev, vr, 0));
    return snd_complete_lifecycle(dev, vr, 0);
}

REGISTER_TEST(SND0051, VIRTIO_PCI_DEVICE_SND,
              test_lifecycle_repeat,
              "Repeat a valid PCM lifecycle",
              VIRTIO_SPEC_V1_2, "5.14.6.6.1");

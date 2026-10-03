/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_rx_status_short(struct virtio_dev *dev,
                                          struct vring *control_vr,
                                          struct vring *rx_vr)
{
    struct virtio_snd_pcm_info info;
    uint32_t stream_id;
    test_result_t result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_INPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;
    struct virtio_snd_pcm_xfer *xfer = vv_alloc_pages(1);
    struct virtio_snd_pcm_status *status = vv_alloc_pages(1);
    uint16_t used_idx = rx_vr->used->idx;
    xfer->stream_id = stream_id;
    memset(status, 0xff, sizeof(*status));
    vring_raw_set_desc(rx_vr, 0, vv_virt_to_phys(xfer), sizeof(*xfer),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(rx_vr, 1, vv_virt_to_phys(status), 4,
                       VRING_DESC_F_WRITE, 0);
    vring_submit(rx_vr, 0);

    return snd_wait_pcm_safe(dev, rx_vr, used_idx, 4, status, 4);
}

REGISTER_TEST_CONTROL_Q(SND0076, VIRTIO_PCI_DEVICE_SND,
                        test_rx_status_short,
                        "Bound RX completion to short response buffer",
                        VIRTIO_SPEC_V1_2, "5.14.6.8", 3);

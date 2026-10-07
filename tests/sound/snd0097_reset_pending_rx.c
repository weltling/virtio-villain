/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_reset_pending_rx(struct virtio_dev *dev,
                                           struct vring *control_vr,
                                           struct vring *rx_vr)
{
    struct virtio_snd_pcm_info info;
    uint32_t stream_id;
    test_result_t result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_INPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;
    result = snd_prepare_stream(dev, control_vr, stream_id);
    if (result != TEST_PASS)
        return result;

    struct virtio_snd_pcm_xfer *xfer = vv_alloc_pages(1);
    uint8_t *response = vv_alloc_pages(1);
    volatile struct vring_used *old_used = rx_vr->used;
    xfer->stream_id = stream_id;
    memset(response, 0xff, 1032);
    vring_raw_set_desc(rx_vr, 0, vv_virt_to_phys(xfer), sizeof(*xfer),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(rx_vr, 1, vv_virt_to_phys(response), 1032,
                       VRING_DESC_F_WRITE, 0);
    vring_submit(rx_vr, 0);
    virtio_pci_kick(dev, rx_vr->queue);

    result = snd_reset_reinit(dev, control_vr, rx_vr);
    if (result != TEST_PASS)
        return result;
    uint16_t reset_used_idx = old_used->idx;
    usleep(50000);
    if (old_used->idx != reset_used_idx)
        TFAIL("pre-reset RX completed after reset");

    result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_INPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;
    result = snd_start_stream(dev, control_vr, stream_id);
    if (result != TEST_PASS)
        return result;
    return snd_submit_pcm_xfer(dev, rx_vr, stream_id, true, 1024);
}

REGISTER_TEST_CONTROL_Q(SND0097, VIRTIO_PCI_DEVICE_SND,
                        test_reset_pending_rx,
                        "Recover after reset with pending PCM capture",
                        VIRTIO_SPEC_V1_2, "2.4.2", 3);

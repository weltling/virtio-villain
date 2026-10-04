/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_rx_over_buffer(struct virtio_dev *dev,
                                         struct vring *control_vr,
                                         struct vring *rx_vr)
{
    struct virtio_snd_pcm_info info;
    struct virtio_snd_pcm_set_params params;
    uint32_t stream_id;
    test_result_t result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_INPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;
    result = snd_make_valid_params(
        dev, control_vr, stream_id, &params, &info);
    if (result != TEST_PASS)
        return result;
    params.buffer_bytes = 1024;
    params.period_bytes = 1024;
    result = snd_expect_ok(dev, control_vr, &params, sizeof(params));
    if (result != TEST_PASS)
        return result;
    result = snd_expect_pcm_command_ok(
        dev, control_vr, stream_id, VIRTIO_SND_R_PCM_PREPARE);
    if (result != TEST_PASS)
        return result;
    result = snd_expect_pcm_command_ok(
        dev, control_vr, stream_id, VIRTIO_SND_R_PCM_START);
    if (result != TEST_PASS)
        return result;

    struct virtio_snd_pcm_xfer *xfer = vv_alloc_pages(1);
    uint8_t *response = vv_alloc_pages(1);
    uint16_t used_idx = rx_vr->used->idx;
    xfer->stream_id = stream_id;
    memset(response, 0xff, 2056);
    vring_raw_set_desc(rx_vr, 0, vv_virt_to_phys(xfer), sizeof(*xfer),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(rx_vr, 1, vv_virt_to_phys(response), 2056,
                       VRING_DESC_F_WRITE, 0);
    vring_submit(rx_vr, 0);

    result = vv_kick_and_wait(dev, rx_vr, 0, VV_TIMEOUT_MS);
    if (result != TEST_PASS)
        return result;
    uint32_t used_len = rx_vr->used->ring[used_idx % rx_vr->size].len;
    if (used_len > 2056)
        TFAIL("RX used length %u exceeds writable capacity 2056", used_len);
    if (used_len < sizeof(struct virtio_snd_pcm_status))
        TFAIL("RX completed with only %u bytes", used_len);
    struct virtio_snd_pcm_status *status =
        (struct virtio_snd_pcm_status *)
        (response + used_len - sizeof(*status));
    if (status->status != VIRTIO_SND_S_OK)
        TFAIL("RX status 0x%08x", status->status);
    return TEST_PASS;
}

REGISTER_TEST_CONTROL_Q(SND0081, VIRTIO_PCI_DEVICE_SND,
                        test_rx_over_buffer,
                        "Safely handle RX larger than configured buffer",
                        VIRTIO_SPEC_V1_2, "5.14.6.8", 3);

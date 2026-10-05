/* SPDX-License-Identifier: Apache-2.0 */
#include "tests/sound/sound_util.h"

static test_result_t test_tx_header_writable(struct virtio_dev *dev,
                                             struct vring *control_vr,
                                             struct vring *tx_vr)
{
    struct virtio_snd_pcm_info info;
    uint32_t stream_id;
    test_result_t result = snd_find_pcm_stream(
        dev, control_vr, VIRTIO_SND_D_OUTPUT, &stream_id, &info);
    if (result != TEST_PASS)
        return result;

    struct virtio_snd_pcm_xfer *xfer = vv_alloc_pages(1);
    struct virtio_snd_pcm_status *status = vv_alloc_pages(1);
    uint16_t used_idx = tx_vr->used->idx;
    xfer->stream_id = stream_id;
    memset(status, 0xff, sizeof(*status));
    vring_raw_set_desc(tx_vr, 0, vv_virt_to_phys(xfer), sizeof(*xfer),
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1);
    vring_raw_set_desc(tx_vr, 1, vv_virt_to_phys(status), sizeof(*status),
                       VRING_DESC_F_WRITE, 0);
    vring_submit(tx_vr, 0);

    return snd_wait_pcm_safe(dev, tx_vr, used_idx,
                             sizeof(*xfer) + sizeof(*status),
                             (struct virtio_snd_pcm_status *)xfer,
                             sizeof(*xfer) + sizeof(*status));
}

REGISTER_TEST_CONTROL_Q(SND0086, VIRTIO_PCI_DEVICE_SND,
                        test_tx_header_writable,
                        "Safely handle writable TX header",
                        VIRTIO_SPEC_V1_2, "2.7.5", 2);

/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_VIRTIO_SND_H
#define VV_VIRTIO_SND_H

#include <stdint.h>

#define VIRTIO_SND_R_JACK_INFO       0x0001
#define VIRTIO_SND_R_PCM_INFO        0x0100
#define VIRTIO_SND_R_PCM_SET_PARAMS  0x0101
#define VIRTIO_SND_R_PCM_PREPARE     0x0102
#define VIRTIO_SND_R_PCM_RELEASE     0x0103
#define VIRTIO_SND_R_PCM_START       0x0104
#define VIRTIO_SND_R_PCM_STOP        0x0105
#define VIRTIO_SND_R_CHMAP_INFO      0x0200

#define VIRTIO_SND_S_OK        0x8000
#define VIRTIO_SND_S_BAD_MSG   0x8001
#define VIRTIO_SND_S_NOT_SUPP  0x8002
#define VIRTIO_SND_S_IO_ERR    0x8003

struct virtio_snd_hdr {
    uint32_t code;
} __attribute__((packed));

struct virtio_snd_query_info {
    struct virtio_snd_hdr hdr;
    uint32_t start_id;
    uint32_t count;
    uint32_t size;
} __attribute__((packed));

struct virtio_snd_jack_info {
    uint32_t hda_fn_nid;
    uint32_t features;
    uint32_t hda_reg_defconf;
    uint32_t hda_reg_caps;
    uint8_t connected;
    uint8_t padding[7];
} __attribute__((packed));

struct virtio_snd_pcm_hdr {
    struct virtio_snd_hdr hdr;
    uint32_t stream_id;
} __attribute__((packed));

struct virtio_snd_pcm_set_params {
    struct virtio_snd_pcm_hdr hdr;
    uint32_t buffer_bytes;
    uint32_t period_bytes;
    uint32_t features;
    uint8_t channels;
    uint8_t format;
    uint8_t rate;
    uint8_t padding;
} __attribute__((packed));

struct virtio_snd_pcm_info {
    uint32_t hda_fn_nid;
    uint32_t features;
    uint64_t formats;
    uint64_t rates;
    uint8_t direction;
    uint8_t channels_min;
    uint8_t channels_max;
    uint8_t padding[5];
} __attribute__((packed));

#define VIRTIO_SND_CHMAP_MAX_SIZE  18

struct virtio_snd_chmap_info {
    uint32_t hda_fn_nid;
    uint8_t direction;
    uint8_t channels;
    uint8_t positions[VIRTIO_SND_CHMAP_MAX_SIZE];
} __attribute__((packed));

struct virtio_snd_config {
    uint32_t jacks;
    uint32_t streams;
    uint32_t chmaps;
} __attribute__((packed));

#endif /* VV_VIRTIO_SND_H */

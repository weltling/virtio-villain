/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0047: net_speed_duplex_no_status
 *
 * Read the speed and duplex configuration fields without having
 * negotiated VIRTIO_NET_F_SPEED_DUPLEX. Tests whether the device
 * provides sensible defaults or traps on unfeature-gated config reads.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_config {
    uint8_t  mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;
    uint8_t  duplex;
} __attribute__((packed));

static test_result_t test_net_speed_no_feature(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    /*
     * Just read the speed/duplex fields from device config.
     * If the device doesn't offer the feature, these fields may
     * not exist - reading them could fault or return garbage.
     * If we get here without crashing, test passes.
     */
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_net_config *ncfg =
        (volatile struct virtio_net_config *)dev->device_cfg;

    /* Read speed - offset 16 in net config */
    volatile uint32_t speed = ncfg->speed;
    volatile uint8_t duplex = ncfg->duplex;
    (void)speed;
    (void)duplex;

    /* If we survived the reads, device handled it gracefully */
    /* Now verify device is still functional with a TX */
    struct {
        uint8_t flags;
        uint8_t gso_type;
        uint16_t hdr_len;
        uint16_t gso_size;
        uint16_t csum_start;
        uint16_t csum_offset;
    } __attribute__((packed)) *hdr = vv_alloc_pages(1);

    uint8_t *frame = vv_alloc_pages(1);
    hdr->flags = 0;
    hdr->gso_type = 0;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;
    memset(frame, 0xFF, 60);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), 10,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 60, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0047, VIRTIO_PCI_DEVICE_NET, test_net_speed_no_feature,
              "Read speed/duplex config without SPEED_DUPLEX feature",
              VIRTIO_SPEC_V1_2, "5.1.4");

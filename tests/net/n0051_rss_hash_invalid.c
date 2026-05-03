/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0051: net_rss_hash_invalid_type
 *
 * Without VIRTIO_NET_F_RSS, set a hash report field in the TX
 * virtio_net_hdr (if the device supports hash reporting). With
 * an invalid hash type value, the device should ignore or reject.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} __attribute__((packed));

#define VIRTIO_NET_F_HASH_REPORT 57
#define VIRTIO_NET_F_RSS         60

static test_result_t test_net_rss_hash_invalid(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* If device doesn't offer RSS or HASH_REPORT, skip */
    cfg->device_feature_select = 1; /* bits 32-63 */
    __sync_synchronize();
    uint32_t offered_hi = cfg->device_feature;
    if (!(offered_hi & (1U << (VIRTIO_NET_F_HASH_REPORT - 32))) &&
        !(offered_hi & (1U << (VIRTIO_NET_F_RSS - 32))))
        return TEST_SKIP;

    /* TX on queue 1 */
    if (cfg->num_queues < 2)
        return TEST_SKIP;

    struct vring txvr;
    vring_alloc(&txvr, 16);
    vring_attach(dev, &txvr, 1);

    /*
     * Use a header with garbage in the extended hash fields.
     * Allocate extra bytes to cover hash_value, hash_report_type.
     */
    uint8_t *rawhdr = vv_alloc_pages(1);
    memset(rawhdr, 0, 20);
    /* Set hash_report_type (offset 13 in extended header) to invalid */
    rawhdr[13] = 0xFF;

    uint8_t *frame = vv_alloc_pages(1);
    memset(frame, 0xFF, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x08; frame[13] = 0x00;
    memset(frame + 14, 0xDD, 46);

    vring_raw_set_desc(&txvr, 0, vv_virt_to_phys(rawhdr), 20,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&txvr, 1, vv_virt_to_phys(frame), 60, 0, 0);
    vring_raw_set_avail(&txvr, 0, 0);
    vring_raw_set_avail_idx(&txvr, 1);

    return vv_kick_and_wait(dev, &txvr, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0051, VIRTIO_PCI_DEVICE_NET, test_net_rss_hash_invalid,
              "TX with invalid hash_report_type in extended header",
              VIRTIO_SPEC_V1_2, "5.1.6.4");

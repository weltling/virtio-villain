/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0109: net_ctrl_mac_table_oversized
 *
 * Submit a MAC table via CTRL_MAC whose entry count exceeds the
 * descriptor length. Spec 5.1.6.5.3 says the device must validate
 * MAC table boundaries. A truncated table must not cause an OOB
 * read in the device model.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_net_ctrl_hdr {
    uint8_t class;
    uint8_t command;
} __attribute__((packed));

#define VIRTIO_NET_CTRL_MAC              1
#define VIRTIO_NET_CTRL_MAC_TABLE_SET    0

static test_result_t test_net_mac_table_oversized(struct virtio_dev *dev,
                                                  struct vring *vr)
{
    uint8_t *page = vv_alloc_pages(1);
    memset(page, 0, 4096);

    struct virtio_net_ctrl_hdr *ctrl = (void *)page;
    ctrl->class = VIRTIO_NET_CTRL_MAC;
    ctrl->command = VIRTIO_NET_CTRL_MAC_TABLE_SET;

    /* Unicast table: claim 1000 entries but only provide header */
    uint32_t *uc_count = (uint32_t *)(page + sizeof(*ctrl));
    *uc_count = 1000;

    /* Multicast table: zero entries */
    uint32_t *mc_count = (uint32_t *)(page + sizeof(*ctrl) + 4);
    *mc_count = 0;

    uint8_t *status = vv_alloc_pages(1);
    *status = 0xFF;

    uint64_t page_phys = vv_virt_to_phys(page);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Header + unicast table (truncated) */
    vring_raw_set_desc(vr, 0, page_phys,
                       sizeof(*ctrl) + 4 + 4,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, VV_QUEUE_LAST, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0109, VIRTIO_PCI_DEVICE_NET, test_net_mac_table_oversized,
                "CTRL_MAC table entry count exceeds descriptor",
                VIRTIO_SPEC_V1_2, "5.1.6.5.3", VV_QUEUE_LAST);

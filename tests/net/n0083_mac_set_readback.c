/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0083: MAC set via ctrl vq then read back from config
 *
 * Spec 5.1.6.5.2 says with VIRTIO_NET_F_CTRL_MAC_ADDR negotiated
 * the driver writes a new MAC via the control vq and the device
 * updates its config space mac field. This test sets a known MAC,
 * waits for the ack, then reads the mac field and verifies the
 * device updated config space accordingly.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>


static test_result_t test_net_mac_set_readback(struct virtio_dev *dev,
                                               struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    static const uint8_t want[6] = {
        0x02, 0x11, 0x22, 0x33, 0x44, 0x55
    };

    (void)vr;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1U << VIRTIO_NET_F_CTRL_MAC_ADDR)))
        return TEST_SKIP;

    if (!dev->device_cfg || dev->device_cfg_length < 6)
        return TEST_SKIP;

    struct vring ctrl_vr;
    vring_alloc(&ctrl_vr, 64);
    vring_attach(dev, &ctrl_vr, (uint16_t)(cfg->num_queues - 1));

    struct virtio_net_ctrl_hdr *hdr = vv_alloc_pages(1);
    uint8_t *mac = vv_alloc_pages(1);
    uint8_t *ack = vv_alloc_pages(1);

    hdr->class = VIRTIO_NET_CTRL_MAC;
    hdr->command = VIRTIO_NET_CTRL_MAC_ADDR_SET;
    memcpy(mac, want, 6);
    *ack = 0xFF;

    vring_raw_set_desc(&ctrl_vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&ctrl_vr, 1, vv_virt_to_phys(mac), 6,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&ctrl_vr, 2, vv_virt_to_phys(ack), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&ctrl_vr, 0, 0);
    vring_raw_set_avail_idx(&ctrl_vr, 1);

    test_result_t r = vv_kick_and_wait(dev, &ctrl_vr,
                                       cfg->num_queues - 1, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (*ack != 0)
        TREJECT("*ack != 0");

    /* Wait briefly for config space to settle and read back */
    usleep(20000);
    __sync_synchronize();
    volatile uint8_t *cfg_mac = (volatile uint8_t *)dev->device_cfg +
                                VIRTIO_NET_CFG_MAC_OFFSET;
    for (int i = 0; i < 6; i++) {
        if (cfg_mac[i] != want[i])
            TFAIL("cfg_mac[i] != want[i]");
    }

    return TEST_PASS;
}

REGISTER_TEST(N0083, VIRTIO_PCI_DEVICE_NET, test_net_mac_set_readback,
              "CTRL_MAC ADDR_SET updates mac field in config space",
              VIRTIO_SPEC_V1_2, "5.1.6.5.2");

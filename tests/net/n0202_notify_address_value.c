/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0202: Dedicated TX notification address with mismatched queue values.
 *
 * Spec 4.1.4.4, 4.1.4.9 and 4.1.5.2.
 *
 * Notify TX queue 1 with queue_select set to 0 and payload 0 or 0xffff.
 * Exercise 16 bit and 32 bit MMIO and PCI configuration access.
 *
 * Malformed notifications may be rejected but a valid kick must recover.
 * Check each used entry and follow every malformed kick with valid traffic.
 *
 * Regression coverage for https://github.com/microsoft/openvmm/pull/4914.
 */
#include "lib/virtio_test.h"
#include "lib/pci.h"

#include <errno.h>

struct notify_target {
    volatile void *address;
    int fd;
    uint8_t cap;
    uint8_t bar;
    uint32_t offset;
};

static test_result_t wait_tx_completion(struct virtio_dev *dev,
                                         struct vring *vr, uint16_t before,
                                         uint16_t head)
{
    int scale = vv_wait_scale > 0 ? vv_wait_scale : 1;
    for (int waited = 0; waited < VV_TIMEOUT_MS * scale; waited++) {
        __sync_synchronize();
        if (vr->used->idx != before)
            break;
        usleep(1000);
    }

    __sync_synchronize();
    uint16_t completed = (uint16_t)(vr->used->idx - before);
    uint8_t status = vv_mmio_read8(&dev->common->device_status);
    if (!(status & VIRTIO_STATUS_DRIVER_OK) ||
        (status & (VIRTIO_STATUS_NEEDS_RESET | VIRTIO_STATUS_FAILED)))
        TWEDGED("device became unhealthy after notification: status=0x%02x",
                status);
    if (completed == 0)
        return TEST_REJECT;
    if (completed != 1)
        TFAIL("one TX request produced %u completions", completed);

    uint32_t id = vr->used->ring[before % vr->size].id;
    if (id != head)
        TFAIL("TX completion id=%u expected=%u", id, head);
    return TEST_PASS;
}

static test_result_t send_notification(struct virtio_dev *dev,
                                        struct vring *vr,
                                        const struct notify_target *target,
                                        unsigned width, uint32_t value,
                                        int via_cfg)
{
    struct virtio_net_hdr_mrg *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);
    uint16_t before = vr->used->idx;
    uint16_t head = (uint16_t)((before % (vr->size / 2)) * 2);

    memset(frame, 0xff, 6);
    memset(frame + 6, 0x02, 6);
    frame[12] = 0x88;
    frame[13] = 0xb5;
    vv_desc(vr, head, hdr, sizeof(*hdr), VRING_DESC_F_NEXT, head + 1);
    vv_desc(vr, head + 1, frame, 60, 0, 0);

    if (via_cfg) {
        pci_cfg_write8(target->fd, target->cap + 4, target->bar);
        pci_cfg_write32(target->fd, target->cap + 8, target->offset);
        pci_cfg_write32(target->fd, target->cap + 12, width);
        if (pci_cfg_read8(target->fd, target->cap + 4) != target->bar ||
            pci_cfg_read32(target->fd, target->cap + 8) != target->offset ||
            pci_cfg_read32(target->fd, target->cap + 12) != width)
            TFAIL("PCI configuration access window did not retain selection");
    }

    vv_log("%s %u bit notification value=0x%04x",
           via_cfg ? "PCI cfg" : "MMIO", width * 8, value);
    vv_mmio_write16(&dev->common->queue_select, 0);
    vring_submit(vr, head);
    if (via_cfg)
        pci_cfg_write32(target->fd, target->cap + 16, value);
    else if (width == 2)
        vv_mmio_write16(target->address, (uint16_t)value);
    else
        vv_mmio_write32(target->address, value);
    __sync_synchronize();

    test_result_t result = wait_tx_completion(dev, vr, before, head);
    if (result != TEST_REJECT)
        return result;

    vv_mmio_write16(target->address, vr->queue);
    __sync_synchronize();
    result = wait_tx_completion(dev, vr, before, head);
    if (result == TEST_REJECT)
        TWEDGED("TX request did not complete after a valid recovery kick");
    if (result != TEST_PASS)
        return result;
    if (width == 2 && value == vr->queue)
        TFAIL("valid %s notification was ignored",
              via_cfg ? "PCI cfg" : "MMIO");
    vv_log("malformed notification was rejected and valid recovery succeeded");
    return TEST_REJECT;
}

static test_result_t exercise_notifications(struct virtio_dev *dev,
                                             struct vring *vr, int fd)
{
    uint8_t cap = pci_cfg_read8(fd, PCI_CAP_PTR) & 0xfc;
    uint8_t notify_cap = 0;
    for (unsigned count = 0; cap && count < 48; count++) {
        if (pci_cfg_read8(fd, cap) == PCI_CAP_ID_VNDR &&
            pci_cfg_read8(fd, cap + 3) == VIRTIO_PCI_CAP_NOTIFY_CFG)
            notify_cap = cap;
        cap = pci_cfg_read8(fd, cap + PCI_CAP_LIST_NEXT) & 0xfc;
    }
    if (cap)
        TFAIL("PCI capability list did not terminate");
    if (!notify_cap)
        TSKIP("notification capability is absent");

    uint32_t multiplier = pci_cfg_read32(fd, notify_cap + 16);
    uint32_t notify_offset = pci_cfg_read32(fd, notify_cap + 8);
    uint32_t notify_length = pci_cfg_read32(fd, notify_cap + 12);
    uint64_t offsets[2];
    for (uint16_t q = 0; q < 2; q++) {
        vv_mmio_write16(&dev->common->queue_select, q);
        __sync_synchronize();
        offsets[q] = (uint64_t)vv_mmio_read16(
            &dev->common->queue_notify_off) * multiplier;
    }
    if (offsets[0] == offsets[1])
        TSKIP("RX and TX share a notification address");
    if (offsets[1] + 2 > notify_length ||
        offsets[1] + notify_offset > UINT32_MAX)
        TFAIL("TX notification slot is outside the advertised region");
    if (offsets[1] + 4 > notify_length ||
        (notify_offset + offsets[1]) % 4)
        TSKIP("TX notification region cannot fit an aligned 32 bit write");

    struct notify_target target = {
        .address = (volatile uint8_t *)dev->notify_base + offsets[1],
        .fd = fd,
        .cap = dev->pci_cfg_cap_offset,
        .bar = pci_cfg_read8(fd, notify_cap + 4),
        .offset = notify_offset + (uint32_t)offsets[1],
    };

    VV_TRY(send_notification(dev, vr, &target, 2, vr->queue, 0));
    VV_TRY(send_notification(dev, vr, &target, 2, vr->queue, 1));

    static const uint32_t values[] = {0, 0xffff};
    int rejected = 0;
    for (unsigned access = 0; access < 4; access++) {
        unsigned width = (access & 1) ? 4 : 2;
        int via_cfg = access >= 2;
        for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
            test_result_t result = send_notification(
                dev, vr, &target, width, values[i], via_cfg);
            if (result != TEST_PASS && result != TEST_REJECT)
                return result;
            if (result == TEST_REJECT)
                rejected = 1;
            VV_TRY(send_notification(dev, vr, &target, 2, vr->queue, 0));
        }
    }
    return rejected ? TEST_REJECT : TEST_PASS;
}

static test_result_t test_net_notify_address_value(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    if (vr->queue != 1 || dev->common->num_queues < 2 || vr->size < 2)
        TSKIP("an RX queue and a TX queue with two descriptors are required");
    if (!dev->pci_cfg_cap_offset)
        TSKIP("PCI configuration access capability is absent");
    if (!dev->notify_base)
        TSKIP("notification region is not mapped");

    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        TFAIL("cannot open PCI configuration: %s", strerror(errno));

    uint8_t cap = dev->pci_cfg_cap_offset;
    uint8_t saved_bar = pci_cfg_read8(fd, cap + 4);
    uint32_t saved_offset = pci_cfg_read32(fd, cap + 8);
    uint32_t saved_length = pci_cfg_read32(fd, cap + 12);
    test_result_t result = exercise_notifications(dev, vr, fd);
    pci_cfg_write8(fd, cap + 4, saved_bar);
    pci_cfg_write32(fd, cap + 8, saved_offset);
    pci_cfg_write32(fd, cap + 12, saved_length);
    close(fd);
    return result;
}

REGISTER_TEST_Q_REQUIRES(N0202, VIRTIO_PCI_DEVICE_NET,
                        test_net_notify_address_value,
                        "TX notify address with mismatched queue values",
                        VIRTIO_SPEC_V1_2, "4.1.4.4", 1, 0, 2);

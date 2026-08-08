/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Fuzz replay guest — reads a fuzz blob from an embedded ELF section,
 * attaches each selected virtio queue, populates it, kicks it, waits
 * briefly, then shuts down.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <unistd.h>

#include "lib/util.h"
#include "lib/virtio_pci.h"
#include "lib/vring.h"
#include "lib/virtio_spec.h"
#include "lib/fuzz_input.h"

/*
 * The fuzz input blob lives in a dedicated ELF section.
 * The orchestrator patches these bytes between runs.
 */
__attribute__((section(".fuzz_input"), aligned(4096), used))
static uint8_t fuzz_blob[FUZZ_BLOB_SIZE] = {0};

struct fuzz_target {
    const struct fuzz_input *hdr;
    const struct fuzz_desc *descs;
    const uint16_t *avail_entries;
    const uint8_t *payload;
    uint32_t payload_len;
    uint16_t device_id;
    uint16_t queue_index;
    uint16_t device_index;
};

struct fuzz_device {
    uint16_t device_id;
    uint16_t max_queue;
    struct virtio_dev dev;
};

static struct fuzz_target targets[FUZZ_MAX_SEGMENTS];
static struct fuzz_device devices[FUZZ_MAX_SEGMENTS];
static struct vring queues[FUZZ_MAX_SEGMENTS][FUZZ_MAX_QUEUE_SIZE];

static void shutdown(void)
{
    fflush(stdout);
    sync();

    /* Use reboot syscall - works on both CH and QEMU */
    reboot(RB_POWER_OFF);
    _exit(0);
}

/* Features to negotiate per device so control queues become live. The
 * net control queue only exists when CTRL_VQ is negotiated. */
static unsigned __int128 fuzz_device_features(uint16_t device_id)
{
    if (device_id == VIRTIO_PCI_DEVICE_NET)
        return (unsigned __int128)1 << VIRTIO_NET_F_CTRL_VQ;
    return 0;
}

int main(void)
{
    if (getpid() != 1) {
        fprintf(stderr, "fuzz: must run as PID 1 inside VM\n");
        return 1;
    }

    /* Minimal /proc for kernel command line (not strictly needed here) */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    const uint8_t *segment;
    uint32_t segment_len;
    uint16_t segment_device;
    uint16_t segment_queue;
    uint16_t target_count = fuzz_blob_segment_count(fuzz_blob, FUZZ_BLOB_SIZE);
    uint16_t device_count = 0;

    if (target_count == 0) {
        printf("FUZZ: invalid blob\n");
        shutdown();
    }

    for (uint16_t i = 0; i < target_count; i++) {
        struct fuzz_target *target = &targets[i];
        if (fuzz_blob_segment(fuzz_blob, FUZZ_BLOB_SIZE, i,
                              &segment_device, &segment_queue,
                              &segment, &segment_len) < 0 ||
            segment_queue >= FUZZ_MAX_QUEUE_SIZE ||
            fuzz_parse(segment, segment_len, &target->hdr, &target->descs,
                       &target->avail_entries, &target->payload,
                       &target->payload_len) < 0) {
            printf("FUZZ: invalid segment\n");
            shutdown();
        }
        target->device_id = segment_device;
        target->queue_index = segment_queue;
        for (uint16_t j = 0; j < i; j++) {
            if (targets[j].device_id == segment_device &&
                targets[j].queue_index == segment_queue) {
                printf("FUZZ: duplicate queue\n");
                shutdown();
            }
        }
        for (uint16_t j = 0; j < device_count; j++) {
            if (devices[j].device_id == segment_device) {
                target->device_index = j;
                goto found_device;
            }
        }
        target->device_index = device_count;
        devices[device_count].device_id = segment_device;
        devices[device_count].max_queue = segment_queue;
        device_count++;
found_device:
        if (segment_queue > devices[target->device_index].max_queue)
            devices[target->device_index].max_queue = segment_queue;
    }

    for (uint16_t d = 0; d < device_count; d++) {
        struct fuzz_device *device = &devices[d];
        if (virtio_pci_find(device->device_id, &device->dev) < 0 ||
            virtio_pci_init_features(
                &device->dev,
                fuzz_device_features(device->device_id)) < 0) {
            printf("FUZZ: device init failed\n");
            shutdown();
        }
        for (uint16_t q = 0; q <= device->max_queue; q++) {
            struct fuzz_target *target = NULL;
            for (uint16_t i = 0; i < target_count; i++) {
                if (targets[i].device_index == d &&
                    targets[i].queue_index == q) {
                    target = &targets[i];
                    break;
                }
            }
            uint16_t qs = target ? target->hdr->queue_size : 16;
            if (qs == 0 || (qs & (qs - 1)) != 0 ||
                qs > FUZZ_MAX_QUEUE_SIZE)
                qs = 16;
            struct vring *vr = &queues[d][q];
            vring_alloc(vr, qs);
            vring_attach(&device->dev, vr, q);
            if (!target)
                continue;

            uint16_t nd = target->hdr->num_descs;
            if (nd == 0 || nd > FUZZ_MAX_DESCS || nd > qs)
                nd = 1;
            uint16_t ac = target->hdr->avail_count;
            if (ac > qs)
                ac = qs;
            uint32_t payload_pages = (target->payload_len + 4095) / 4096;
            if (payload_pages == 0)
                payload_pages = 1;
            uint8_t *payload_buf = vv_alloc_pages(payload_pages);
            memcpy(payload_buf, target->payload, target->payload_len);
            uint64_t payload_phys = vv_virt_to_phys(payload_buf);
            uint32_t offset = 0;
            for (uint16_t i = 0; i < nd; i++) {
                uint32_t len = target->descs[i].len;
                if (offset + len > payload_pages * 4096)
                    len = payload_pages * 4096 - offset;
                vring_raw_set_desc(vr, i, payload_phys + offset, len,
                                   target->descs[i].flags,
                                   target->descs[i].next);
                offset += len;
                if (offset >= payload_pages * 4096)
                    offset = 0;
            }
            for (uint16_t i = 0; i < ac; i++)
                vring_raw_set_avail(vr, i, target->avail_entries[i]);
            vring_raw_set_avail_idx(vr, target->hdr->avail_idx);
        }
        device->dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
        __sync_synchronize();
    }

    /* Drive the blk config read and writeback cache mode branches by
     * reading the capacity and toggling the WCE byte before the kicks. */
    for (uint16_t d = 0; d < device_count; d++) {
        struct fuzz_device *device = &devices[d];
        if (device->device_id != VIRTIO_PCI_DEVICE_BLK)
            continue;
        volatile uint8_t *cfg = device->dev.device_cfg;
        if (!cfg)
            continue;
        if (device->dev.device_cfg_length >= 8) {
            volatile uint64_t *capacity = (volatile uint64_t *)cfg;
            (void)*capacity;
        }
        if (device->dev.device_cfg_length > VIRTIO_BLK_CFG_WCE_OFFSET) {
            uint8_t wce = cfg[VIRTIO_BLK_CFG_WCE_OFFSET];
            cfg[VIRTIO_BLK_CFG_WCE_OFFSET] = wce ? 0 : 1;
        }
    }

    for (uint16_t i = 0; i < target_count; i++)
        virtio_pci_kick(&devices[targets[i].device_index].dev,
                        targets[i].queue_index);

    /* Wait for device to process */
    usleep(50000);

    /* Reset and re-activate each device so the device reset, the queue
     * teardown, and a second activate run in addition to the first
     * activate. init_features clears device_status from DRIVER_OK, which
     * is the reset the device model tears its queues down on. The vring
     * memory is preserved, so re-attaching and kicking replays it. */
    for (uint16_t d = 0; d < device_count; d++) {
        struct fuzz_device *device = &devices[d];
        if (virtio_pci_init_features(
                &device->dev,
                fuzz_device_features(device->device_id)) < 0)
            continue;
        for (uint16_t q = 0; q <= device->max_queue; q++)
            vring_attach(&device->dev, &queues[d][q], q);
        device->dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
        __sync_synchronize();
    }

    for (uint16_t i = 0; i < target_count; i++)
        virtio_pci_kick(&devices[targets[i].device_index].dev,
                        targets[i].queue_index);

    usleep(50000);

    printf("FUZZ: done\n");
    shutdown();
    return 0;
}

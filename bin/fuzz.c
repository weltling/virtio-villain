/* SPDX-License-Identifier: Apache-2.0 */
/*
 * Fuzz replay guest — reads a fuzz blob from an embedded ELF section,
 * sets up a virtio-blk device, populates the vring according to the blob,
 * kicks the device, waits briefly, then shuts down.
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
#include "lib/fuzz_input.h"

/*
 * Target device and queue are selectable at build time so the same
 * harness can drive different virtio devices. The default targets
 * virtio-blk on queue 0. A scsi build sets the request queue index.
 */
#ifndef FUZZ_DEVICE_ID
#define FUZZ_DEVICE_ID VIRTIO_PCI_DEVICE_BLK
#endif
#ifndef FUZZ_QUEUE
#define FUZZ_QUEUE 0
#endif

/*
 * The fuzz input blob lives in a dedicated ELF section.
 * The orchestrator patches these bytes between runs.
 */
__attribute__((section(".fuzz_input"), aligned(4096), used))
static uint8_t fuzz_blob[FUZZ_BLOB_SIZE] = {0};

static void shutdown(void)
{
    fflush(stdout);
    sync();

    /* Use reboot syscall - works on both CH and QEMU */
    reboot(RB_POWER_OFF);
    _exit(0);
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

    /* Parse the fuzz blob */
    const struct fuzz_input *hdr;
    const struct fuzz_desc *descs;
    const uint16_t *avail_entries;
    const uint8_t *payload;
    const uint8_t *segment;
    uint32_t payload_len;
    uint32_t segment_len;
    uint16_t segment_device;
    uint16_t segment_queue;

    if (fuzz_blob_first(fuzz_blob, FUZZ_BLOB_SIZE,
                         &segment_device, &segment_queue,
                         &segment, &segment_len) < 0 ||
        fuzz_parse(segment, segment_len,
                   &hdr, &descs, &avail_entries,
                   &payload, &payload_len) < 0) {
        printf("FUZZ: invalid blob\n");
        shutdown();
    }

    (void)segment_device;
    (void)segment_queue;

    /* Sanity: need at least 1 descriptor and valid queue size */
    uint16_t qs = hdr->queue_size;
    if (qs == 0 || (qs & (qs - 1)) != 0) /* must be power of 2 */
        qs = 16;
    if (qs > FUZZ_MAX_QUEUE_SIZE)
        qs = FUZZ_MAX_QUEUE_SIZE;

    uint16_t nd = hdr->num_descs;
    if (nd == 0 || nd > FUZZ_MAX_DESCS || nd > qs)
        nd = 1;

    uint16_t ac = hdr->avail_count;
    if (ac > qs)
        ac = qs;

    /* Find and init the target virtio device */
    struct virtio_dev dev;
    if (virtio_pci_find(FUZZ_DEVICE_ID, &dev) < 0) {
        printf("FUZZ: no target device\n");
        shutdown();
    }
    if (virtio_pci_init(&dev) < 0) {
        printf("FUZZ: init failed\n");
        shutdown();
    }

    /*
     * Attach filler vrings for any queues before the target so the
     * target queue is enabled. A scsi request queue at index 2 needs
     * the control and event queues to exist first.
     */
#if FUZZ_QUEUE > 0
    struct vring filler[FUZZ_QUEUE];
    for (uint16_t q = 0; q < FUZZ_QUEUE; q++) {
        vring_alloc(&filler[q], 16);
        vring_attach(&dev, &filler[q], q);
    }
#endif

    /* Allocate the fuzzed vring on the target queue */
    struct vring vr;
    vring_alloc(&vr, qs);
    vring_attach(&dev, &vr, FUZZ_QUEUE);

    /* DRIVER_OK */
    dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    /*
     * Allocate payload pages. We put all payload in a contiguous region
     * and let descriptors point into it by cumulative offset.
     */
    uint32_t payload_pages = (payload_len + 4095) / 4096;
    if (payload_pages == 0)
        payload_pages = 1;
    uint8_t *payload_buf = vv_alloc_pages(payload_pages);
    memcpy(payload_buf, payload, payload_len);
    uint64_t payload_phys = vv_virt_to_phys(payload_buf);

    /*
     * Populate descriptor table.
     * Each descriptor's addr points into the payload at cumulative offset.
     */
    uint32_t offset = 0;
    for (uint16_t i = 0; i < nd; i++) {
        uint32_t len = descs[i].len;
        uint16_t flags = descs[i].flags;
        uint16_t next = descs[i].next;

        /* Clamp len so addr+len doesn't exceed payload allocation */
        if (offset + len > payload_pages * 4096)
            len = payload_pages * 4096 - offset;

        uint64_t addr = payload_phys + offset;
        vring_raw_set_desc(&vr, i, addr, len, flags, next);

        offset += len;
        if (offset >= payload_pages * 4096)
            offset = 0; /* wrap around to keep going */
    }

    /* Populate available ring */
    for (uint16_t i = 0; i < ac; i++)
        vring_raw_set_avail(&vr, i, avail_entries[i]);

    vring_raw_set_avail_idx(&vr, hdr->avail_idx);

    /* Kick the target queue */
    virtio_pci_kick(&dev, FUZZ_QUEUE);

    /* Wait for device to process */
    usleep(50000);

    printf("FUZZ: done\n");
    shutdown();
    return 0;
}

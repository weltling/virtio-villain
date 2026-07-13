/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0019: FUSE INTERRUPT for a request submitted on the normal queue.
 *
 * Spec 5.11.6.1: After establishing the FUSE session, submit a
 * FUSE GETATTR on the request queue, then immediately submit a
 * FUSE INTERRUPT on the hiprio queue targeting the same unique.
 * The device must either complete the original request or return
 * EINTR, without deadlock.
 * Adapted from QEMU virtio-9p-test flush_success pattern.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/fuse.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_interrupt_inflight(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    if (cfg->num_queues < 2)
        return TEST_SKIP;

    /* Phase 1: FUSE_INIT on requestq (vr = queue 1) */
    uint8_t *init_page = vv_alloc_pages(1);
    memset(init_page, 0, 4096);

    struct fuse_in_header *ihdr = (struct fuse_in_header *)init_page;
    struct fuse_init_in *init =
        (struct fuse_init_in *)(init_page + sizeof(*ihdr));

    ihdr->len    = sizeof(*ihdr) + sizeof(*init);
    ihdr->opcode = FUSE_INIT;
    ihdr->unique = 1;
    init->major  = 7;
    init->minor  = 31;
    init->max_readahead = 65536;

    uint64_t init_phys = vv_virt_to_phys(init_page);

    vring_raw_set_desc(vr, 0, init_phys, ihdr->len,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, init_phys + 512, 256,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    /* Phase 2: Submit GETATTR for root inode as the inflight request */
    uint8_t *ga_page = vv_alloc_pages(1);
    memset(ga_page, 0, 4096);

    struct fuse_in_header *ghdr = (struct fuse_in_header *)ga_page;
    struct fuse_getattr_in *ga =
        (struct fuse_getattr_in *)(ga_page + sizeof(*ghdr));

    ghdr->len    = sizeof(*ghdr) + sizeof(*ga);
    ghdr->opcode = FUSE_GETATTR;
    ghdr->unique = 77;
    ghdr->nodeid = 1; /* root inode */
    ga->getattr_flags = 0;
    ga->fh = 0;

    uint64_t ga_phys = vv_virt_to_phys(ga_page);

    vring_raw_set_desc(vr, 2, ga_phys, ghdr->len,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, ga_phys + 512, 256,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 2);
    vring_raw_set_avail_idx(vr, 2);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);

    /* Phase 3: Immediately submit INTERRUPT on hiprio (queue 0) */
    struct vring hiprio;
    vring_alloc(&hiprio, 16);
    vring_attach(dev, &hiprio, 0);

    uint8_t *intr_page = vv_alloc_pages(1);
    memset(intr_page, 0, 4096);

    struct fuse_in_header *intr_hdr = (struct fuse_in_header *)intr_page;
    struct fuse_interrupt_in *intr =
        (struct fuse_interrupt_in *)(intr_page + sizeof(*intr_hdr));

    intr_hdr->len    = sizeof(*intr_hdr) + sizeof(*intr);
    intr_hdr->opcode = FUSE_INTERRUPT;
    intr_hdr->unique = 0;
    intr->unique     = 77; /* target the GETATTR request */

    uint64_t intr_phys = vv_virt_to_phys(intr_page);

    vring_raw_set_desc(&hiprio, 0, intr_phys, intr_hdr->len, 0, 0);
    vring_raw_set_avail(&hiprio, 0, 0);
    vring_raw_set_avail_idx(&hiprio, 1);

    __sync_synchronize();
    virtio_pci_kick(dev, 0);

    /* Wait for GETATTR to complete (either normally or with EINTR) */
    int elapsed = 0;
    while (elapsed < VV_TIMEOUT_MS * 1000) {
        usleep(10000);
        __sync_synchronize();
        if ((uint16_t)(vr->used->idx - 1) >= 1)
            return TEST_PASS;
        if (hiprio.used->idx >= 1)
            return TEST_PASS;
        elapsed += 10000;
    }

    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("device reset after INTERRUPT");
    TREJECT("no completion on either queue after INTERRUPT");
}

REGISTER_TEST_Q_REQUIRES(F0019, VIRTIO_PCI_DEVICE_FS, test_fs_interrupt_inflight,
              "FUSE INTERRUPT targeting inflight request",
              VIRTIO_SPEC_V1_2, "5.11.6.1", 1,
              0, 2);

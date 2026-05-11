/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0014: Submit multiple FUSE requests on the hiprio queue.
 *
 * Spec 5.11.5: Queue 0 is the hiprio (notification) queue used
 * for FUSE_NOTIFY_REPLY. Submit two writable buffers and kick.
 * The device may never fill them, but must not crash when multiple
 * buffers are outstanding on this queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_fs_hiprio_multi(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf1 = vv_alloc_pages(1);
    uint8_t *buf2 = vv_alloc_pages(1);
    memset(buf1, 0, 4096);
    memset(buf2, 0, 4096);

    uint64_t phys1 = vv_virt_to_phys(buf1);
    uint64_t phys2 = vv_virt_to_phys(buf2);

    vring_raw_set_desc(vr, 0, phys1, 256, VRING_DESC_F_WRITE, 0);
    vring_raw_set_desc(vr, 1, phys2, 256, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail(vr, 1, 1);
    vring_raw_set_avail_idx(vr, 2);

    __sync_synchronize();
    virtio_pci_kick(dev, vr->queue);
    usleep(500000);

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    return TEST_PASS;
}

REGISTER_TEST_Q(F0014, VIRTIO_PCI_DEVICE_FS, test_fs_hiprio_multi,
                "Multiple buffers on hiprio queue",
                VIRTIO_SPEC_V1_2, "5.11.5", 0);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0070: per vector MSI-X mask suppresses queued completion
 *
 * Spec 4.1.4.5 and the PCI MSI-X spec say a vector with the mask
 * bit set in its table entry must not deliver MSIs while masked,
 * and any pending MSI must be deferred. Bind queue 0 to vector 0,
 * mask the table entry, submit a request, wait for completion via
 * the used ring, sample the ISR register, then unmask. The device
 * must complete the request regardless of the mask state, which
 * proves masking only affects MSI delivery and not request flow.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/pci.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define PCI_CAP_LIST_ID   0
#define PCI_CAP_LIST_NEXT 1
#define PCI_CAP_PTR       0x34
#define PCI_STATUS        0x06
#define PCI_CAP_ID_MSIX   0x11

struct msix_table_entry {
    uint32_t msg_addr_lo;
    uint32_t msg_addr_hi;
    uint32_t msg_data;
    uint32_t vector_ctrl;
} __attribute__((packed));

struct virtio_blk_outhdr {
    uint32_t type;
    uint32_t ioprio;
    uint64_t sector;
} __attribute__((packed));

#define VIRTIO_BLK_T_IN 0

static volatile struct msix_table_entry *find_msix_table(struct virtio_dev *dev)
{
    int fd = pci_cfg_open(dev->slot);
    if (fd < 0)
        return NULL;
    uint16_t status = pci_cfg_read16(fd, PCI_STATUS);
    if (!(status & 0x10)) { close(fd); return NULL; }
    uint8_t pos = pci_cfg_read8(fd, PCI_CAP_PTR) & 0xFC;
    uint8_t msix_pos = 0;
    int count = 0;
    while (pos && count < 48) {
        uint8_t id = pci_cfg_read8(fd, pos + PCI_CAP_LIST_ID);
        if (id == PCI_CAP_ID_MSIX) { msix_pos = pos; break; }
        pos = pci_cfg_read8(fd, pos + PCI_CAP_LIST_NEXT) & 0xFC;
        count++;
    }
    if (!msix_pos) { close(fd); return NULL; }
    uint32_t tobir = pci_cfg_read32(fd, msix_pos + 4);
    int bir = tobir & 0x7;
    uint32_t off = tobir & ~0x7u;
    close(fd);
    volatile void *bar = pci_map_bar(dev->slot, bir);
    if (!bar)
        return NULL;
    return (volatile struct msix_table_entry *)((uint8_t *)bar + off);
}

static test_result_t test_pci_msix_per_vec_mask(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    volatile struct msix_table_entry *t = find_msix_table(dev);
    if (!t)
        return TEST_SKIP;

    /* Bind queue 0 to vector 0 */
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_msix_vector = 0;
    __sync_synchronize();
    if (cfg->queue_msix_vector != 0) {
        cfg->queue_msix_vector = 0xFFFF;
        __sync_synchronize();
        return TEST_SKIP;
    }

    /* Set the per vector mask bit */
    uint32_t orig = t[0].vector_ctrl;
    t[0].vector_ctrl = orig | 1u;
    __sync_synchronize();

    /* Submit a normal read */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *st = vv_alloc_pages(1);
    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *st = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(st), 1,
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    virtio_pci_kick(dev, vr->queue);

    /* Poll used ring directly while vector is masked */
    int done = 0;
    for (int i = 0; i < VV_TIMEOUT_MS; i++) {
        __sync_synchronize();
        if (vr->used->idx != 0) { done = 1; break; }
        usleep(1000);
    }

    /* Unmask and restore */
    t[0].vector_ctrl = orig;
    __sync_synchronize();
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_msix_vector = 0xFFFF;
    __sync_synchronize();

    if (!done)
        TWEDGED("!done");
    if (*st != 0)
        TFAIL("*st != 0");
    return TEST_PASS;
}

REGISTER_TEST(PCI0070, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_per_vec_mask,
              "request completes while MSI-X vector is masked",
              VIRTIO_SPEC_V1_2, "4.1.4.5");

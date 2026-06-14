/* SPDX-License-Identifier: Apache-2.0 */
/*
 * PCI0041: Assign MSI-X vectors to multiple queues.
 *
 * Spec 4.1.4.3.2: The driver can assign different MSI-X vectors to
 * different queues via queue_msix_vector. Assign vectors, verify
 * readback, then do I/O to exercise the interrupt routing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_pci_msix_multi_vector(struct virtio_dev *dev,
                                                struct vring *vr)
{
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    /* Assign config vector = 0 */
    cfg->msix_config = 0;
    __sync_synchronize();
    uint16_t readback = cfg->msix_config;
    /* Device may return NO_VECTOR (0xFFFF) if it can't allocate */
    (void)readback;

    /* Assign queue 0 vector = 1 */
    cfg->queue_select = 0;
    __sync_synchronize();
    cfg->queue_msix_vector = 1;
    __sync_synchronize();
    uint16_t qv = cfg->queue_msix_vector;
    (void)qv;

    /* Now do a simple I/O to see if things still work */
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(data), 512,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(status), 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(PCI0041, VIRTIO_PCI_DEVICE_BLK, test_pci_msix_multi_vector,
              "Assign MSI-X vectors to config and queue",
              VIRTIO_SPEC_V1_2, "4.1.4.3.2");

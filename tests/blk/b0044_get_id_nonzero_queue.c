/* SPDX-License-Identifier: Apache-2.0 */
/*
 * B0044: blk_get_id_nonzero_queue
 *
 * Submit a GET_ID request via a non-zero queue index (multiqueue).
 * Some VMMs only handle GET_ID on queue 0. If multiqueue is
 * advertised, the device should handle GET_ID on any request queue.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_blk_get_id_nonzero_queue(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr; /* We use queue 1 instead */

    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    uint16_t nq = cfg->num_queues;
    if (nq < 2)
        return TEST_SKIP;

    /* Set up queue 1 */
    struct vring vr1;
    vring_alloc(&vr1, 16);
    vring_attach(dev, &vr1, 1);

    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *id_buf = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_GET_ID;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;
    memset(id_buf, 0, 20);

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t id_phys = vv_virt_to_phys(id_buf);
    uint64_t status_phys = vv_virt_to_phys(status);

    vring_raw_set_desc(&vr1, 0, hdr_phys, sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&vr1, 1, id_phys, 20,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 2);
    vring_raw_set_desc(&vr1, 2, status_phys, 1,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(&vr1, 0, 0);
    vring_raw_set_avail_idx(&vr1, 1);

    return vv_kick_and_wait(dev, &vr1, 1, VV_TIMEOUT_MS);
}

REGISTER_TEST(B0044, VIRTIO_PCI_DEVICE_BLK, test_blk_get_id_nonzero_queue,
              "GET_ID on non-zero queue (multiqueue)",
              VIRTIO_SPEC_V1_2, "5.2.6.1");

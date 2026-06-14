/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0021: packed_toggle_flags_mid_chain
 *
 * In a multi-descriptor chain, set the first descriptor as available
 * but toggle the AVAIL flag off on the second descriptor. The device
 * should either reject this as a malformed chain or treat the second
 * descriptor as not yet available.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_packed_toggle_mid_chain(struct virtio_dev *dev,
                                                  struct vring_packed *vr)
{
    struct virtio_blk_outhdr *hdr = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    hdr->type = VIRTIO_BLK_T_IN;
    hdr->ioprio = 0;
    hdr->sector = 0;
    *status = 0xFF;

    uint64_t hdr_phys = vv_virt_to_phys(hdr);
    uint64_t data_phys = vv_virt_to_phys(data);
    uint64_t status_phys = vv_virt_to_phys(status);

    /*
     * Desc 0: available (correct flags), NEXT set
     * Desc 1: WRONG avail flag (inverted), NEXT set
     * Desc 2: available (correct flags), WRITE
     */
    uint16_t avail_flag = vr->wrap_counter ? (1 << 7) : 0;
    uint16_t used_flag = vr->wrap_counter ? 0 : (1 << 15);
    uint16_t wrong_avail = vr->wrap_counter ? 0 : (1 << 7);
    uint16_t wrong_used = vr->wrap_counter ? (1 << 15) : 0;

    /* Desc 0: header - properly available */
    vr->desc[0].addr = hdr_phys;
    vr->desc[0].len = sizeof(*hdr);
    vr->desc[0].id = 0;

    /* Desc 1: data - WRONG avail/used bits */
    vr->desc[1].addr = data_phys;
    vr->desc[1].len = 512;
    vr->desc[1].id = 1;
    vr->desc[1].flags = VRING_PACKED_DESC_F_NEXT |
                         VRING_PACKED_DESC_F_WRITE |
                         wrong_avail | wrong_used;

    /* Desc 2: status - properly available */
    vr->desc[2].addr = status_phys;
    vr->desc[2].len = 1;
    vr->desc[2].id = 2;
    vr->desc[2].flags = VRING_PACKED_DESC_F_WRITE | avail_flag | used_flag;

    __sync_synchronize();
    /* Write desc 0 flags last (makes chain "available") */
    vr->desc[0].flags = VRING_PACKED_DESC_F_NEXT | avail_flag | used_flag;
    __sync_synchronize();

    return vv_kick_and_wait_packed(dev, vr, 0, 2, vr->wrap_counter,
                                   VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0021, VIRTIO_PCI_DEVICE_BLK, test_packed_toggle_mid_chain,
                     "Packed chain with wrong AVAIL flag on middle descriptor",
                     VIRTIO_SPEC_V1_2, "2.8.6");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * P0080: indirect_write_and_indirect_flags
 *
 * A packed descriptor carrying both the INDIRECT and WRITE flags. The
 * spec says an indirect descriptor must not also be device writable.
 * The device must reject the combination rather than treating the
 * indirect table page as a writable data buffer.
 */
#include "tests/packed/packed_util.h"

static test_result_t test_packed_write_and_indirect(struct virtio_dev *dev,
                                                   struct vring_packed *vr)
{
    uint64_t table_phys = pk_indirect_blk_table();

    uint16_t a0 = vr->next_avail;
    uint8_t w0 = vr->wrap_counter;

    pk_append(vr, table_phys, 3 * sizeof(struct vring_packed_desc), a0,
              VRING_PACKED_DESC_F_INDIRECT | VRING_PACKED_DESC_F_WRITE);

    return vv_kick_and_wait_packed(dev, vr, 0, a0, w0, VV_TIMEOUT_MS);
}

REGISTER_TEST_PACKED(P0080, VIRTIO_PCI_DEVICE_BLK,
                     test_packed_write_and_indirect,
                     "Packed descriptor with both INDIRECT and WRITE flags",
                     VIRTIO_SPEC_V1_2, "2.8.6");

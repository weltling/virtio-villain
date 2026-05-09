/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0099: vring_indirect_len_huge
 *
 * Point an indirect descriptor at an oversized table whose len
 * field declares 4096 entries times queue_size. Spec 2.7.5.3
 * caps the table at queue_size entries. The device must reject
 * the chain rather than reading megabytes of fanout.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_vring_indirect_huge(struct virtio_dev *dev,
                                              struct vring *vr)
{
    struct vring_desc *ind = vv_alloc_pages(8);
    memset(ind, 0, 8 * 4096);

    /* Plausible head pointing at first slot to avoid trivial reject */
    uint8_t *data = vv_alloc_pages(1);
    ind[0].addr = vv_virt_to_phys(data);
    ind[0].len = 1;
    ind[0].flags = VRING_DESC_F_WRITE;
    ind[0].next = 0;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(ind),
                       (uint32_t)(4096u * 64u),
                       VRING_DESC_F_INDIRECT, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(T0099, VIRTIO_PCI_DEVICE_BLK, test_vring_indirect_huge,
              "Indirect table length huge multiple of qsize",
              VIRTIO_SPEC_V1_2, "2.7.5.3");

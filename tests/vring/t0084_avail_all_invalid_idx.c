/* SPDX-License-Identifier: Apache-2.0 */
/*
 * T0084: Avail ring with all entries set to same invalid index (spec 2.7.6)
 *
 * Fill the entire avail ring with the same out-of-bounds descriptor
 * index (queue_size). Then advance avail idx by the full ring size.
 * The device must detect the invalid indices without crashing.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_avail_all_invalid(struct virtio_dev *dev,
                                            struct vring *vr)
{
    uint16_t qsz = vr->size;

    /* Fill all avail ring entries with index = queue_size (invalid OOB) */
    for (uint16_t i = 0; i < qsz; i++)
        vr->avail->ring[i] = qsz; /* out of bounds */

    /* Advance avail idx by full ring */
    __sync_synchronize();
    vr->avail->idx = qsz;
    __sync_synchronize();

    virtio_pci_kick(dev, 0);
    usleep(VV_TIMEOUT_MS * 1000);

    /* Device should reject all entries */
    volatile struct virtio_pci_common_cfg *cfg = dev->common;
    __sync_synchronize();
    if (cfg->device_status == 0)
        TWEDGED("cfg->device_status == 0");
    TREJECT("no device response within timeout");
}

REGISTER_TEST(T0084, VIRTIO_PCI_DEVICE_BLK, test_avail_all_invalid,
              "Avail ring fully filled with out-of-bounds descriptor indices",
              VIRTIO_SPEC_V1_2, "2.7.6");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0017: Console transmit indirect without feature negotiated.
 *
 * Submit a transmitq descriptor with VRING_DESC_F_INDIRECT set.
 * The harness negotiates zero features so VIRTIO_F_INDIRECT_DESC
 * is not active. Per spec 2.7.7 the driver MUST NOT use indirect
 * descriptors without the feature. The device must reject the
 * request rather than dereference the indirect table.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_indirect_no_feature(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    struct vring_desc *itab = vv_alloc_pages(1);
    char *payload = (char *)itab + 256;
    memcpy(payload, "vv\n", 3);

    uint64_t itab_phys = vv_virt_to_phys(itab);
    uint64_t payload_phys = itab_phys + 256;

    itab[0].addr  = payload_phys;
    itab[0].len   = 3;
    itab[0].flags = 0;
    itab[0].next  = 0;

    vring_raw_set_desc(vr, 0, itab_phys, sizeof(*itab),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0017, VIRTIO_PCI_DEVICE_CONSOLE,
                test_console_tx_indirect_no_feature,
                "Console transmit indirect without feature",
                VIRTIO_SPEC_V1_2, "2.7.7", 1);

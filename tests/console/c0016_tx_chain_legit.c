/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0016: Console transmit multi-segment chain.
 *
 * Submit a legitimate 3-segment read-only chain on the transmitq.
 * The device must consume the whole chain and advance the used
 * ring once.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_chain_legit(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    char *buf = (char *)vv_alloc_pages(1);
    memcpy(buf, "vv", 2);
    memcpy(buf + 32, "ch", 2);
    memcpy(buf + 64, "ain\n", 4);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 2, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, buf_phys + 32, 2, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, buf_phys + 64, 4, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0016, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_chain_legit,
                "Console transmit multi-segment chain",
                VIRTIO_SPEC_V1_2, "5.3", 1);

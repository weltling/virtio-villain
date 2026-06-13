/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0001: Console basic transmit.
 *
 * Submit a single read-only descriptor with a small payload to the
 * transmitq (queue 1). The device must consume the descriptor and
 * advance the used ring without crashing.
 *
 * Cloud Hypervisor console: 2 queues, single-port mode.
 *   queue 0 = receiveq (device writes), queue 1 = transmitq.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_basic(struct virtio_dev *dev,
                                           struct vring *vr)
{
    char *buf = (char *)vv_alloc_pages(1);
    memcpy(buf, "vv\n", 3);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    /* TX descriptor: read-only payload sent from guest to device */
    vring_raw_set_desc(vr, 0, buf_phys, 3, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0001, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_basic,
                "Console basic transmit",
                VIRTIO_SPEC_V1_2, "5.3", 1);

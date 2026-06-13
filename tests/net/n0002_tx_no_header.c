/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0002: net_tx_no_header
 *
 * Submit a TX descriptor shorter than sizeof(struct virtio_net_hdr).
 * The device must reject or error rather than reading past the buffer.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_no_header(struct virtio_dev *dev,
                                           struct vring *vr)
{
    /* A 4-byte buffer - too short for the 10-byte virtio_net_hdr */
    uint8_t *buf = vv_alloc_pages(1);
    memset(buf, 0, 4);

    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 4, 0, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(N0002, VIRTIO_PCI_DEVICE_NET, test_net_tx_no_header,
              "TX descriptor shorter than virtio_net_hdr",
              VIRTIO_SPEC_V1_2, "5.1.6");

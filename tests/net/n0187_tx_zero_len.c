/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0187: tx_zero_len
 *
 * A transmit chain has a zero length frame descriptor after the
 * header. The device must accept or refuse the empty payload without
 * harm to the host.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_tx_zero_len(struct virtio_dev *dev,
                                          struct vring *vr)
{
    struct virtio_net_hdr *hdr = vv_alloc_pages(1);
    uint8_t *frame = vv_alloc_pages(1);
    memset(hdr, 0, sizeof(*hdr));

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(hdr), sizeof(*hdr),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(frame), 0, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0187, VIRTIO_PCI_DEVICE_NET, test_net_tx_zero_len,
                "Transmit chain with a zero length frame descriptor",
                VIRTIO_SPEC_V1_2, "2.7.5", 1);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0014: Console receive multi-segment writable chain.
 *
 * Submit a legitimate two-descriptor writable chain on the
 * receiveq. The device must walk both segments correctly when
 * host input arrives and must not crash on chain handling. On a
 * quiet console the expected outcome is REJECT.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_rx_chain_writable(struct virtio_dev *dev,
                                                    struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(1);
    uint64_t buf_phys = vv_virt_to_phys(buf);

    vring_raw_set_desc(vr, 0, buf_phys, 32,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 1);
    vring_raw_set_desc(vr, 1, buf_phys + 32, 32, VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(C0014, VIRTIO_PCI_DEVICE_CONSOLE, test_console_rx_chain_writable,
              "Console receive multi-segment writable chain",
              VIRTIO_SPEC_V1_2, "5.3");

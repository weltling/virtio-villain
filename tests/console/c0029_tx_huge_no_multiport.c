/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0029: console_tx_huge_no_multiport
 *
 * Submit a TX descriptor with a length of 64 KiB. With MULTIPORT
 * off there is no max_nr_ports field and the device must not
 * misinterpret the length as a port index or panic. Spec 5.3
 * requires the device to consume or drop the descriptor and
 * remain alive.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_console_tx_huge(struct virtio_dev *dev,
                                          struct vring *vr)
{
    uint8_t *buf = vv_alloc_pages(16);
    memset(buf, 0x42, 16 * 4096);

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(buf), 16 * 4096, 0, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(C0029, VIRTIO_PCI_DEVICE_CONSOLE, test_console_tx_huge,
                "TX huge length descriptor without MULTIPORT",
                VIRTIO_SPEC_V1_2, "5.3", 1);

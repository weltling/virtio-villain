/* SPDX-License-Identifier: Apache-2.0 */
/*
 * N0022: net_ctrl_undefined_class
 *
 * Submit a control queue command with an undefined class value (0xFF).
 * The device should reject or ignore unknown control classes rather
 * than crashing or executing arbitrary handler code.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t test_net_ctrl_undefined_class(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    struct virtio_net_ctrl_hdr *ctrl = vv_alloc_pages(1);
    uint8_t *status = vv_alloc_pages(1);

    /* Undefined class 0xFF, command 0 */
    ctrl->class = 0xFF;
    ctrl->command = 0;
    *status = 0xFF;

    uint64_t ctrl_phys = vv_virt_to_phys(ctrl);
    uint64_t status_phys = vv_virt_to_phys(status);

    /* Control command: header (readable) -> status (writable) */
    vring_raw_set_desc(vr, 0, ctrl_phys, sizeof(*ctrl), VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, status_phys, 1, VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST_Q(N0022, VIRTIO_PCI_DEVICE_NET, test_net_ctrl_undefined_class,
              "Control queue command with undefined class",
              VIRTIO_SPEC_V1_2, "5.1.6.5", VV_QUEUE_LAST);

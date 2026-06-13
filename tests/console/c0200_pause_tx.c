/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0200: Console TX survives a pause and resume cycle.
 *
 * Submit a small TX payload, sleep so the sidecar can pause and
 * resume, submit another TX payload. Both must drain on the
 * transmitq.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>
#include <unistd.h>

static test_result_t do_tx(struct virtio_dev *dev, struct vring *vr,
                           const char *msg, size_t mlen)
{
    char *buf = (char *)vv_alloc_pages(1);
    memcpy(buf, msg, mlen);
    uint64_t buf_phys = vv_virt_to_phys(buf);
    uint16_t idx = vr->avail->idx;
    vring_raw_set_desc(vr, idx % vr->size, buf_phys, (uint32_t)mlen, 0, 0);
    vring_raw_set_avail(vr, idx % vr->size, idx % vr->size);
    vring_raw_set_avail_idx(vr, idx + 1);
    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS * 4);
}

static test_result_t test_console_pause_tx(struct virtio_dev *dev,
                                           struct vring *vr)
{
    test_result_t r = do_tx(dev, vr, "vv-a\n", 5);
    if (r != TEST_PASS)
        return r;
    sleep(2);
    return do_tx(dev, vr, "vv-b\n", 5);
}

REGISTER_TEST_Q(C0200, VIRTIO_PCI_DEVICE_CONSOLE, test_console_pause_tx,
                "Console transmitq survives a pause and resume cycle",
                VIRTIO_SPEC_V1_2, "5.3", 1);

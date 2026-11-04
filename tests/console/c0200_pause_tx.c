/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0200: Console TX survives a pause and resume cycle.
 *
 * Submit a small TX payload, sleep so the sidecar can pause and
 * resume, submit another TX payload. Both must drain on the
 * transmitq.
 */
#include "lib/virtio_test.h"
#include "lib/mmio_helpers.h"

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
    VV_TRY(do_tx(dev, vr, "vv-a\n", 5));
    sleep(2);
    return do_tx(dev, vr, "vv-b\n", 5);
}

REGISTER_TEST_Q(C0200, VIRTIO_PCI_DEVICE_CONSOLE, test_console_pause_tx,
                "Console transmitq survives a pause and resume cycle",
                VIRTIO_SPEC_V1_2, "5.3", 1);

static test_result_t do_tx_mmio(struct virtio_mmio_dev *dev,
                               struct vring *ring, const char *message)
{
    char *buffer = vv_alloc_pages(1);
    size_t length = strlen(message);
    memcpy(buffer, message, length);
    vv_desc(ring, 0, buffer, length, 0, 0);
    uint16_t before = ring->used->idx;
    vring_submit(ring, 0);
    virtio_mmio_kick(dev, 1);
    for (int elapsed = 0; elapsed < VV_TIMEOUT_MS * 4 * vv_wait_scale; elapsed++) {
        __sync_synchronize();
        if (ring->used->idx != before)
            return TEST_PASS;
        usleep(1000);
    }
    TWEDGED("MMIO console TX did not complete");
}

static test_result_t test_console_pause_mmio(struct virtio_mmio_dev *dev)
{
    if (virtio_mmio_init(dev) < 0)
        TFAIL("MMIO console feature negotiation failed");
    struct vring ring;
    test_result_t result = mmio_attach_queue(dev, &ring, 1);
    if (result != TEST_PASS)
        return result;
    mmio_write32(dev, VIRTIO_MMIO_STATUS,
                 mmio_read32(dev, VIRTIO_MMIO_STATUS)
                 | VIRTIO_MMIO_STATUS_DRIVER_OK);
    result = do_tx_mmio(dev, &ring, "vv-a\n");
    if (result != TEST_PASS)
        return result;
    puts("vv-mmio-ready");
    fflush(stdout);
    sleep(2);
    return do_tx_mmio(dev, &ring, "vv-b\n");
}

REGISTER_MMIO_VARIANT(C0200, 3, test_console_pause_mmio);

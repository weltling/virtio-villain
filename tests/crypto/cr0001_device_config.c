/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0001: crypto_device_config
 *
 * Spec 5.9.4: the virtio-crypto device configuration exposes the
 * hardware ready status, the number of data queues, the supported
 * service mask and the algorithm masks. With the device found and
 * negotiated by the harness, read the config and verify the device is
 * ready, offers at least one data queue and advertises at least one
 * service, and that it reached DRIVER_OK without setting NEEDS_RESET.
 * CH has no crypto model, so this skips there and runs under QEMU.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

static test_result_t test_crypto_device_config(struct virtio_dev *dev,
                                               struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;

    uint8_t status = dev->common->device_status;
    if (!(status & VIRTIO_STATUS_DRIVER_OK))
        TFAIL("DRIVER_OK clear after crypto negotiation (status 0x%02x)",
              status);
    if (status & VIRTIO_STATUS_NEEDS_RESET)
        TFAIL("device set NEEDS_RESET after crypto negotiation");

    if (!(cfg->status & VIRTIO_CRYPTO_S_HW_READY))
        TFAIL("crypto config status 0x%08x lacks HW_READY", cfg->status);

    if (cfg->max_dataqueues < 1)
        TFAIL("max_dataqueues is %u, expected at least 1",
              cfg->max_dataqueues);

    if (cfg->crypto_services == 0)
        TFAIL("crypto_services mask is zero, no service advertised");

    return TEST_PASS;
}

REGISTER_TEST(CR0001, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_device_config,
              "Crypto config is ready with a data queue and a service",
              VIRTIO_SPEC_V1_2, "5.9.4");

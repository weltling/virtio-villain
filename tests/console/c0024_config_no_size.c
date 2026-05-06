/* SPDX-License-Identifier: Apache-2.0 */
/*
 * C0024: Console config readable without F_SIZE.
 *
 * The harness negotiates zero features so VIRTIO_CONSOLE_F_SIZE is
 * inactive. Per spec 5.3.4 cols and rows fields are valid only
 * when F_SIZE is negotiated, but the config region itself must
 * still be readable. Read the first 4 bytes (cols, rows) and
 * confirm the device responds to MMIO without a fault.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"

#include <string.h>
#include <unistd.h>

struct virtio_console_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} __attribute__((packed));

static test_result_t test_console_config_no_size(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg || dev->device_cfg_length < 4)
        return TEST_SKIP;

    volatile struct virtio_console_config *cfg =
        (volatile struct virtio_console_config *)dev->device_cfg;

    /* Read cols, rows. Values are undefined without F_SIZE but
     * the read itself must complete. */
    uint16_t cols = cfg->cols;
    uint16_t rows = cfg->rows;
    (void)cols;
    (void)rows;

    /* Device must remain alive after the config read. */
    __sync_synchronize();
    if (dev->common->device_status == 0)
        TWEDGED("dev->common->device_status == 0");

    return TEST_PASS;
}

REGISTER_TEST(C0024, VIRTIO_PCI_DEVICE_CONSOLE, test_console_config_no_size,
              "Console config readable without F_SIZE",
              VIRTIO_SPEC_V1_2, "5.3.4");

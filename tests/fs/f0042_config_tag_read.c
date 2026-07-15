/* SPDX-License-Identifier: Apache-2.0 */
/*
 * F0042: read virtio-fs config tag field.
 *
 * Spec 5.11.4: The device config starts with a 36 byte tag field
 * (NUL padded UTF-8 string). Read it and verify it is not empty
 * and contains only printable ASCII or NUL padding.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

static test_result_t test_fs_config_tag(struct virtio_dev *dev,
                                        struct vring *vr)
{
    (void)vr;

    if (!dev->device_cfg || dev->device_cfg_length < 36)
        return TEST_SKIP;

    volatile uint8_t *tag = (volatile uint8_t *)dev->device_cfg;
    uint8_t t[36];
    for (int i = 0; i < 36; i++)
        t[i] = tag[i];

    /* Must not be all zeros (empty tag) */
    int all_zero = 1;
    for (int i = 0; i < 36; i++) {
        if (t[i] != 0) { all_zero = 0; break; }
    }
    if (all_zero)
        TFAIL("tag is empty (all zeros)");

    /* Verify printable ASCII or NUL padding after first NUL */
    int past_nul = 0;
    for (int i = 0; i < 36; i++) {
        if (t[i] == 0) {
            past_nul = 1;
            continue;
        }
        if (past_nul)
            TFAIL("non-NUL byte 0x%02x at offset %d after NUL", t[i], i);
        if (t[i] < 0x20 || t[i] > 0x7E)
            TFAIL("non-printable byte 0x%02x at offset %d", t[i], i);
    }

    return TEST_PASS;
}

REGISTER_TEST(F0042, VIRTIO_PCI_DEVICE_FS, test_fs_config_tag,
              "Read virtio-fs config tag and validate format",
              VIRTIO_SPEC_V1_2, "5.11.4");

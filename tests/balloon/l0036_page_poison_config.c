/* SPDX-License-Identifier: Apache-2.0 */
/*
 * L0036: balloon poison_val config field readable.
 *
 * Spec 5.5.4: when VIRTIO_BALLOON_F_PAGE_POISON is negotiated the
 * config space contains a poison_val field at offset 12 (after
 * num_pages, actual, and free_page_hint_cmd_id). Read it and
 * verify the access does not crash the device. The value is a
 * guest-written field indicating the poison byte pattern used
 * when freeing pages.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <unistd.h>

#define BALLOON_CFG_POISON_VAL_OFFSET 12

static test_result_t test_balloon_page_poison(struct virtio_dev *dev,
                                              struct vring *vr)
{
    (void)vr;
    volatile struct virtio_pci_common_cfg *cfg = dev->common;

    cfg->device_feature_select = 0;
    __sync_synchronize();
    if (!(cfg->device_feature & (1u << VIRTIO_BALLOON_F_PAGE_POISON)))
        return TEST_SKIP;

    if (!dev->device_cfg)
        return TEST_SKIP;
    if (dev->device_cfg_length <= BALLOON_CFG_POISON_VAL_OFFSET + 3)
        return TEST_SKIP;

    volatile uint32_t *poison_val = (volatile uint32_t *)
        ((char *)dev->device_cfg + BALLOON_CFG_POISON_VAL_OFFSET);

    /* Read the current poison value */
    uint32_t val = *poison_val;
    (void)val;

    /* Write a known poison pattern and read it back */
    *poison_val = 0xAAAAAAAA;
    __sync_synchronize();
    usleep(10000);

    uint32_t readback = *poison_val;
    if (readback != 0xAAAAAAAA)
        TFAIL("poison_val readback 0x%08x, expected 0xAAAAAAAA",
              readback);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(L0036, VIRTIO_PCI_DEVICE_BALLOON,
              test_balloon_page_poison,
              "Read and write poison_val config field",
              VIRTIO_SPEC_V1_2, "5.5.4",
              (1ULL << VIRTIO_BALLOON_F_PAGE_POISON), 0);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0103: IOMMU probe validate response status and properties.
 *
 * Spec 5.13.6.5: a PROBE request has a fixed device-readable part
 * (head, endpoint, reserved[64]) followed by a device-writable
 * properties area of probe_size bytes and the tail. probe_size is
 * device specific and read from the device config (spec 5.13.4);
 * the tail therefore sits at offset probe_size within the writable
 * buffer, not at a fixed offset. Verify: (1) tail.status is S_OK,
 * (2) the device wrote into the properties area, and (3) the first
 * property type code is NONE (empty) or RESV_MEM.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

#define VIRTIO_IOMMU_PROBE_T_NONE     0
#define VIRTIO_IOMMU_PROBE_T_RESV_MEM 1

/* Fixed device-readable prefix: head(4) + endpoint(4) + reserved[64]. */
#define PROBE_HEADER_LEN 72

static test_result_t test_iommu_probe_validate(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (dev->device_cfg == NULL)
        return TEST_SKIP;

    volatile struct virtio_iommu_config *cfg =
        (volatile struct virtio_iommu_config *)dev->device_cfg;
    uint32_t probe_size = cfg->probe_size;
    if (probe_size == 0)
        return TEST_SKIP; /* device exposes no properties area */

    /*
     * Request buffer laid out per spec 5.13.6.5:
     *   [0 .. 72)                head, endpoint, reserved   (readable)
     *   [72 .. 72 + probe_size)  properties                 (writable)
     *   [72 + probe_size .. +4)  tail                       (writable)
     */
    uint8_t *buf = vv_alloc_pages((PROBE_HEADER_LEN + probe_size +
                                   sizeof(struct virtio_iommu_req_tail) +
                                   4095) / 4096);
    memset(buf, 0, PROBE_HEADER_LEN);

    struct virtio_iommu_req_head *head = (struct virtio_iommu_req_head *)buf;
    head->type = VIRTIO_IOMMU_T_PROBE;
    *(uint32_t *)(buf + sizeof(*head)) = 0; /* endpoint 0 */

    uint8_t *props = buf + PROBE_HEADER_LEN;
    struct virtio_iommu_req_tail *tail =
        (struct virtio_iommu_req_tail *)(props + probe_size);

    /* Canary so we can detect that the device wrote the properties. */
    memset(props, 0xFE, probe_size);
    tail->status = 0xFF;

    uint64_t buf_phys = vv_virt_to_phys(buf);
    size_t out_len = probe_size + sizeof(*tail);

    vring_raw_set_desc(vr, 0, buf_phys, PROBE_HEADER_LEN,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, buf_phys + PROBE_HEADER_LEN, out_len,
                       VRING_DESC_F_WRITE, 0);

    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;

    if (tail->status != VIRTIO_IOMMU_S_OK)
        TFAIL("probe status %u, expected 0 (S_OK)", tail->status);

    int all_canary = 1;
    for (uint32_t i = 0; i < probe_size; i++) {
        if (props[i] != 0xFE) {
            all_canary = 0;
            break;
        }
    }
    if (all_canary)
        TFAIL("properties area unchanged (all 0xFE canary)");

    /* First two bytes are the property type (le16). */
    uint16_t first_type = props[0] | ((uint16_t)props[1] << 8);
    if (first_type != VIRTIO_IOMMU_PROBE_T_NONE &&
        first_type != VIRTIO_IOMMU_PROBE_T_RESV_MEM)
        TFAIL("first property type %u unexpected", first_type);

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(I0103, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_probe_validate,
              "Probe validates response status and property format",
              VIRTIO_SPEC_V1_2, "5.13.6.5",
              (1ULL << VIRTIO_IOMMU_F_PROBE), 0);

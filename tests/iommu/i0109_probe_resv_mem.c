/* SPDX-License-Identifier: Apache-2.0 */
/*
 * I0109: IOMMU probe RESV_MEM property is well formed.
 *
 * Spec 5.13.6.5: the probe properties area is a list of TLVs. Each
 * starts with a 4 byte header (le16 type in the low 12 bits, le16
 * length of the value that follows) and the next property sits
 * length + 4 bytes on. A RESV_MEM property (type 1) carries subtype,
 * start and end, so its length is 20. Walk the list and, for every
 * RESV_MEM property, verify length is 20, the subtype is RESERVED or
 * MSI, the range is non-empty, and the entry fits in probe_size.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_iommu.h"

#include <string.h>
#include <unistd.h>

#define PROBE_T_MASK          0x0fff
#define PROBE_T_NONE          0
#define PROBE_T_RESV_MEM      1
#define RESV_MEM_VALUE_LEN    20   /* subtype(1)+reserved(3)+start(8)+end(8) */
#define RESV_MEM_T_RESERVED   0
#define RESV_MEM_T_MSI        1
#define PROBE_HEADER_LEN      72   /* head(4) + endpoint(4) + reserved[64] */

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint64_t rd64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)p[i] << (8 * i);
    return v;
}

static test_result_t test_iommu_probe_resv_mem(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (dev->device_cfg == NULL)
        return TEST_SKIP;

    volatile struct virtio_iommu_config *cfg =
        (volatile struct virtio_iommu_config *)dev->device_cfg;
    uint32_t probe_size = cfg->probe_size;
    if (probe_size < 4)
        return TEST_SKIP;

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
    tail->status = 0xFF;

    uint64_t buf_phys = vv_virt_to_phys(buf);
    vring_raw_set_desc(vr, 0, buf_phys, PROBE_HEADER_LEN,
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, buf_phys + PROBE_HEADER_LEN,
                       probe_size + sizeof(*tail), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (tail->status != VIRTIO_IOMMU_S_OK)
        TFAIL("probe status %u, expected 0 (S_OK)", tail->status);

    /* Walk the property list. */
    int resv_mem_seen = 0;
    uint32_t off = 0;
    while (off + 4 <= probe_size) {
        uint16_t type = rd16(props + off) & PROBE_T_MASK;
        uint16_t length = rd16(props + off + 2);

        if (type == PROBE_T_NONE)
            break; /* empty terminator ends the list */

        if ((uint64_t)off + 4 + length > probe_size)
            TFAIL("property at %u overruns probe_size (len %u)", off, length);

        if (type == PROBE_T_RESV_MEM) {
            if (length != RESV_MEM_VALUE_LEN)
                TFAIL("RESV_MEM length %u, expected %u", length,
                      RESV_MEM_VALUE_LEN);
            uint8_t subtype = props[off + 4];
            if (subtype != RESV_MEM_T_RESERVED && subtype != RESV_MEM_T_MSI)
                TFAIL("RESV_MEM subtype %u unexpected", subtype);
            uint64_t start = rd64(props + off + 8);
            uint64_t end = rd64(props + off + 16);
            if (end < start)
                TFAIL("RESV_MEM end 0x%llx < start 0x%llx",
                      (unsigned long long)end, (unsigned long long)start);
            resv_mem_seen = 1;
        }

        off += (uint32_t)length + 4;
    }

    if (!resv_mem_seen)
        return TEST_SKIP; /* device exposed no RESV_MEM property */

    return TEST_PASS;
}

REGISTER_TEST_REQUIRES(I0109, VIRTIO_PCI_DEVICE_IOMMU,
              test_iommu_probe_resv_mem,
              "Probe RESV_MEM property is well formed",
              VIRTIO_SPEC_V1_2, "5.13.6.5",
              (1ULL << VIRTIO_IOMMU_F_PROBE), 0);

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0034: crypto_indirect_table_overlong
 *
 * Fault injection. Spec 2.7.5.3.1 caps an indirect descriptor table at
 * the queue size. A table that declares far more entries than the queue
 * holds must be rejected rather than walked into an unbounded loop or an
 * out of bounds read. Post a data request through an indirect table that
 * declares 64 entries on a 16 deep queue and verify the host survives.
 * The device outcome is up to it: PASS, REJECT and WEDGED are all
 * acceptable as long as the next test can still run. Most valuable under
 * an ASan VMM. Skips on Cloud Hypervisor, when the cipher service is not
 * advertised, and when indirect descriptors are not negotiated.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define INDIRECT_OVERLONG_ENTRIES 64

static test_result_t test_crypto_indirect_overlong(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;
    if (!virtio_pci_feature_offered(dev, VIRTIO_F_INDIRECT_DESC))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *buf = vv_alloc_pages(1);
    struct vring_desc *table = vv_alloc_pages(1);

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(buf, 0, 64);

    /* Build an oversized indirect table chained 0 -> 1 -> ... -> N-1. */
    memset(table, 0, INDIRECT_OVERLONG_ENTRIES * sizeof(*table));
    for (int i = 0; i < INDIRECT_OVERLONG_ENTRIES; i++) {
        table[i].addr = i == 0 ? vv_virt_to_phys(dreq) : vv_virt_to_phys(buf);
        table[i].len = i == 0 ? sizeof(*dreq) : 64;
        int last = i == INDIRECT_OVERLONG_ENTRIES - 1;
        table[i].flags = last ? 0 : VRING_DESC_F_NEXT;
        table[i].next = (uint16_t)(i + 1);
    }

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(table),
                       INDIRECT_OVERLONG_ENTRIES * (uint32_t)sizeof(*table),
                       VRING_DESC_F_INDIRECT, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on an oversized indirect table");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST_REQUIRES(CR0034, VIRTIO_PCI_DEVICE_CRYPTO,
                       test_crypto_indirect_overlong,
                       "An oversized indirect table keeps the host alive",
                       VIRTIO_SPEC_V1_2, "2.7.5.3.1",
                       (1ULL << VIRTIO_F_INDIRECT_DESC), 0);

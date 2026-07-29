/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0051: crypto_indirect_next_flag
 *
 * Fault injection. Spec 2.7.5.3.1 forbids a descriptor from setting both
 * the indirect and next flags. A device that follows the next pointer of
 * an indirect descriptor reads a descriptor that should not exist and
 * may run off the ring. Post a data request whose indirect descriptor
 * also sets the next flag and verify the host survives. The device
 * outcome is up to it: PASS, REJECT and WEDGED are all acceptable as
 * long as the next test can still run. Most valuable under an ASan VMM.
 * Skips on Cloud Hypervisor, when the cipher service is not advertised,
 * and when indirect descriptors are not negotiated.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_indirect_next_flag(struct virtio_dev *dev,
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
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    struct vring_desc *table = vv_alloc_pages(1);

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(iv, 0, 16);
    memset(src, 0x41, 16);
    memset(dst, 0, 16);
    inhdr->status = 0xFF;

    memset(table, 0, 5 * sizeof(*table));
    table[0].addr = vv_virt_to_phys(dreq);
    table[0].len = sizeof(*dreq);
    table[0].flags = VRING_DESC_F_NEXT;
    table[0].next = 1;
    table[1].addr = vv_virt_to_phys(iv);
    table[1].len = 16;
    table[1].flags = VRING_DESC_F_NEXT;
    table[1].next = 2;
    table[2].addr = vv_virt_to_phys(src);
    table[2].len = 16;
    table[2].flags = VRING_DESC_F_NEXT;
    table[2].next = 3;
    table[3].addr = vv_virt_to_phys(dst);
    table[3].len = 16;
    table[3].flags = VRING_DESC_F_NEXT | VRING_DESC_F_WRITE;
    table[3].next = 4;
    table[4].addr = vv_virt_to_phys(inhdr);
    table[4].len = sizeof(*inhdr);
    table[4].flags = VRING_DESC_F_WRITE;
    table[4].next = 0;

    /* Indirect descriptor that also sets the next flag, which the spec
     * forbids. The stale next index points into the main ring. */
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(table),
                       5 * (uint32_t)sizeof(*table),
                       VRING_DESC_F_INDIRECT | VRING_DESC_F_NEXT, 1);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on an indirect descriptor with next");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST_REQUIRES(CR0051, VIRTIO_PCI_DEVICE_CRYPTO,
                       test_crypto_indirect_next_flag,
                       "An indirect descriptor with the next flag keeps the host alive",
                       VIRTIO_SPEC_V1_2, "2.7.5.3.1",
                       (1ULL << VIRTIO_F_INDIRECT_DESC), 0);

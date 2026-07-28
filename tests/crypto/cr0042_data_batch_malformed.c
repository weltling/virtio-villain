/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0042: crypto_data_batch_malformed
 *
 * Fault injection. Spec 2.7.13 lets a driver make several requests
 * available at once. A burst of malformed encrypts submitted together
 * must not let one bad request corrupt the handling of the rest or wedge
 * the host. Post four encrypts with an oversized source length in one
 * batch and verify the host survives. The device outcome is up to it:
 * PASS, REJECT and WEDGED are all acceptable as long as the next test
 * can still run. Most valuable under an ASan VMM. Skips on Cloud
 * Hypervisor and when the cipher service is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define CRYPTO_BATCH 4

static test_result_t test_crypto_batch_malformed(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 0;
    dreq->u.sym_cipher.para.src_data_len = 0xffffffffu;
    dreq->u.sym_cipher.para.dst_data_len = 0;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(src, 0x41, 16);
    inhdr->status = 0xFF;

    /* Each batched request is a two descriptor chain sharing buffers. */
    for (int i = 0; i < CRYPTO_BATCH; i++) {
        uint16_t d0 = (uint16_t)(i * 2);
        uint16_t d1 = (uint16_t)(i * 2 + 1);
        vring_raw_set_desc(vr, d0, vv_virt_to_phys(dreq), sizeof(*dreq),
                           VRING_DESC_F_NEXT, d1);
        vring_raw_set_desc(vr, d1, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(vr, (uint16_t)i, d0);
    }
    vring_raw_set_avail_idx(vr, CRYPTO_BATCH);

    test_result_t r = vv_kick_and_wait_n(dev, vr, 0, CRYPTO_BATCH,
                                         VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a malformed request batch");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0042, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_batch_malformed,
              "A batch of malformed encrypts keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.8");

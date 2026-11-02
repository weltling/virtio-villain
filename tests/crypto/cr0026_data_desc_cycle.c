/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0026: crypto_data_desc_cycle
 *
 * Fault injection. Spec 5.9.8 requests are built from a descriptor chain
 * linked by the next field. A chain that forms a cycle must not send the
 * device into an unbounded walk that hangs or crashes the host. Post a
 * data request whose first two descriptors point at each other and
 * verify the host survives. The device outcome is up to it: PASS, REJECT
 * and WEDGED are all acceptable as long as the next test can still run.
 * Most valuable under an ASan VMM. Skips on Cloud Hypervisor and when
 * the cipher service is not advertised.
 */
#include "lib/virtio_test.h"

static test_result_t test_crypto_data_desc_cycle(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *buf = vv_alloc_pages(1);

    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;

    /* Descriptor 0 links to 1, descriptor 1 links back to 0. */
    vv_desc(vr, 0, dreq, sizeof(*dreq), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, buf, 64, VRING_DESC_F_NEXT, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a cyclic descriptor chain");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0026, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_data_desc_cycle,
              "A cyclic descriptor chain keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.8");

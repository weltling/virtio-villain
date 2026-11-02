/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0031: crypto_data_no_status
 *
 * Fault injection. Spec 5.9.8 has the device write a completion status
 * into a device writable descriptor at the tail of the chain. A chain
 * with no writable descriptor at all leaves the device nowhere to store
 * the result. The device must reject the request rather than write to a
 * read only buffer or crash the host. Post an encrypt whose whole chain
 * is read only and verify the host survives. The device outcome is up to
 * it: PASS, REJECT and WEDGED are all acceptable as long as the next
 * test can still run. Most valuable under an ASan VMM. Skips on Cloud
 * Hypervisor and when the cipher service is not advertised.
 */
#include "lib/virtio_test.h"

static test_result_t test_crypto_data_no_status(struct virtio_dev *dev,
                                                struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);

    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(src, 0x41, 16);

    /* Whole chain read only, no place for the device to write status. */
    vv_desc(vr, 0, dreq, sizeof(*dreq), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, iv, 16, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, src, 16, VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, dst, 16, 0, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a chain with no status buffer");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0031, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_data_no_status,
              "A data chain with no status buffer keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.8");

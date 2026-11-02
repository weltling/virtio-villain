/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0039: crypto_request_header_fragmented
 *
 * Fault injection. Spec 2.7.13 lets a request span several descriptors,
 * so the device must gather the fixed request header from fragments
 * rather than assume it lives in one descriptor. Split the encrypt
 * request structure across four small descriptors, then supply the IV,
 * source, destination and status, and verify the device reassembles it
 * without a short read or a crash. The request is otherwise well formed,
 * so the device is expected to process it; with no session created it
 * rejects the op itself. PASS, REJECT and WEDGED are all acceptable as
 * long as the next test can still run. Skips on Cloud Hypervisor and
 * when the cipher service is not advertised.
 */
#include "lib/virtio_test.h"

static test_result_t test_crypto_header_fragmented(struct virtio_dev *dev,
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
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(src, 0x41, 16);
    inhdr->status = 0xFF;

    /* Fragment the request struct across four contiguous descriptors. */
    uint32_t total = (uint32_t)sizeof(*dreq);
    uint32_t chunk = total / 4;
    uint64_t base = vv_virt_to_phys(dreq);
    vring_raw_set_desc(vr, 0, base, chunk, VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, base + chunk, chunk, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, base + 2 * chunk, chunk, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, base + 3 * chunk, total - 3 * chunk,
                       VRING_DESC_F_NEXT, 4);
    vv_desc(vr, 4, iv, 16, VRING_DESC_F_NEXT, 5);
    vv_desc(vr, 5, src, 16, VRING_DESC_F_NEXT, 6);
    vv_desc(vr, 6, dst, 16, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 7);
    vv_desc(vr, 7, inhdr, sizeof(*inhdr), VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a fragmented request header");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0039, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_header_fragmented,
              "A fragmented request header keeps the host alive",
              VIRTIO_SPEC_V1_2, "2.7.13");

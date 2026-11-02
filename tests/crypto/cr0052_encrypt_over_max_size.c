/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0052: crypto_encrypt_over_max_size
 *
 * Fault injection. Spec 5.9.4 advertises max_size, the largest request
 * the device accepts. A cipher request whose source length is one past
 * max_size must be rejected by that cap rather than accepted. Post an
 * encrypt with src_data_len set just past the advertised max_size over a
 * small source and verify the host survives. The device outcome is up to
 * it: PASS, REJECT and WEDGED are all acceptable as long as the next
 * test can still run. Most valuable under an ASan VMM. Skips on Cloud
 * Hypervisor, when the cipher service is not advertised, and when
 * max_size does not fit a request length.
 */
#include "lib/virtio_test.h"

static test_result_t test_crypto_over_max_size(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    uint64_t max_size = cfg->max_size;
    if (max_size == 0 || max_size >= 0xffffffffu)
        return TEST_SKIP;
    uint32_t over = (uint32_t)max_size + 1u;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = over;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(src, 0x41, 16);
    inhdr->status = 0xFF;

    vv_desc(vr, 0, dreq, sizeof(*dreq), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, iv, 16, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, src, 16, VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, dst, 16, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vv_desc(vr, 4, inhdr, sizeof(*inhdr), VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a request past max_size");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0052, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_over_max_size,
              "A request past the advertised max size keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.4");

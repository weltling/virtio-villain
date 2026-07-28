/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0012: crypto_cipher_distinct_keys
 *
 * Spec 5.9.8: the cipher output depends on the session key, so two
 * sessions created with different keys must transform the same
 * plaintext into different ciphertext. Create two AES-CBC encrypt
 * sessions with distinct keys, encrypt the same 16 byte block under the
 * same IV in each, and verify the two ciphertexts differ. Skips on
 * Cloud Hypervisor, when the cipher service or AES-CBC is not
 * advertised, and when the backend cannot create the session; passes
 * under a QEMU with a crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t create_session(struct virtio_dev *dev, struct vring *cvr,
                                    uint16_t slot, uint8_t key_byte,
                                    uint64_t *session_id_out)
{
    struct virtio_crypto_op_ctrl_req *creq = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(creq, 0, sizeof(*creq));
    creq->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
    creq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    creq->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    creq->u.sym_create.para.keylen = 16;
    creq->u.sym_create.para.op = VIRTIO_CRYPTO_OP_ENCRYPT;
    creq->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(key, key_byte, 16);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(cvr, 0, vv_virt_to_phys(creq), sizeof(*creq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(cvr, 1, vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(cvr, slot, 0);
    vring_raw_set_avail_idx(cvr, (uint16_t)(slot + 1));

    test_result_t r = vv_kick_and_wait(dev, cvr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP ||
        sin->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (sin->status != VIRTIO_CRYPTO_OK)
        TFAIL("create session status %u, expected OK", sin->status);
    *session_id_out = sin->session_id;
    return TEST_PASS;
}

static test_result_t encrypt_block(struct virtio_dev *dev, struct vring *vr,
                                   uint16_t slot, uint64_t session_id,
                                   const uint8_t *iv_in, const uint8_t *src_in,
                                   uint8_t *dst_out)
{
    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = session_id;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memcpy(iv, iv_in, 16);
    memcpy(src, src_in, 16);
    memset(dst, 0, 16);
    inhdr->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(iv), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(src), 16,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(dst), 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, slot, 0);
    vring_raw_set_avail_idx(vr, (uint16_t)(slot + 1));

    test_result_t r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        TFAIL("encrypt status %u, expected OK", inhdr->status);
    memcpy(dst_out, dst, 16);
    return TEST_PASS;
}

static void destroy_session(struct virtio_dev *dev, struct vring *cvr,
                            uint16_t slot, uint64_t session_id)
{
    struct virtio_crypto_op_ctrl_req *creq = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    memset(creq, 0, sizeof(*creq));
    creq->header.opcode = VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION;
    creq->u.destroy.session_id = session_id;
    inhdr->status = 0xFF;
    vring_raw_set_desc(cvr, 0, vv_virt_to_phys(creq), sizeof(*creq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(cvr, 1, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(cvr, slot, 0);
    vring_raw_set_avail_idx(cvr, (uint16_t)(slot + 1));
    (void)vv_kick_and_wait(dev, cvr, 0, VV_TIMEOUT_MS);
}

static test_result_t test_crypto_distinct_keys(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;

    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;
    if (!(cfg->cipher_algo_l & (1u << VIRTIO_CRYPTO_CIPHER_AES_CBC)))
        return TEST_SKIP;

    /* The control queue is the last queue, at index max_dataqueues. */
    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    uint64_t sid_a = 0, sid_b = 0;
    test_result_t r = create_session(dev, &cvr, 0, 0x11, &sid_a);
    if (r != TEST_PASS)
        return r;
    r = create_session(dev, &cvr, 1, 0x22, &sid_b);
    if (r != TEST_PASS)
        return r;

    uint8_t iv[16], plaintext[16], ct_a[16], ct_b[16];
    memset(iv, 0x00, sizeof(iv));
    memset(plaintext, 0x41, sizeof(plaintext));

    r = encrypt_block(dev, vr, 0, sid_a, iv, plaintext, ct_a);
    if (r != TEST_PASS)
        return r;
    r = encrypt_block(dev, vr, 1, sid_b, iv, plaintext, ct_b);
    if (r != TEST_PASS)
        return r;

    if (memcmp(ct_a, ct_b, 16) == 0)
        TFAIL("different keys produced identical ciphertext");

    destroy_session(dev, &cvr, 2, sid_a);
    destroy_session(dev, &cvr, 3, sid_b);

    return TEST_PASS;
}

REGISTER_TEST(CR0012, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_distinct_keys,
              "Cipher output differs for two sessions with distinct keys",
              VIRTIO_SPEC_V1_2, "5.9.8");

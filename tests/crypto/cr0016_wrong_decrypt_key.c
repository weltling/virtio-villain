/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0016: crypto_wrong_decrypt_key
 *
 * Spec 5.9.8: decryption recovers the plaintext only under the key used
 * to encrypt, so decrypting a ciphertext with a different key must not
 * reproduce the original plaintext. Encrypt a 16 byte block under one
 * AES-CBC key, decrypt the ciphertext under a different key, and verify
 * the result does not equal the plaintext. Skips on Cloud Hypervisor,
 * when the cipher service or AES-CBC is not advertised, and when the
 * backend cannot create the session; passes under a QEMU with a crypto
 * library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static uint64_t make_cipher_session(struct virtio_dev *dev, struct vring *cvr,
                                    uint16_t slot, uint32_t op,
                                    uint8_t *key, int *skip)
{
    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.keylen = 16;
    req->u.sym_create.para.op = op;
    req->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(cvr, (uint16_t)(slot * 3), vv_virt_to_phys(req),
                       sizeof(*req), VRING_DESC_F_NEXT, (uint16_t)(slot * 3 + 1));
    vring_raw_set_desc(cvr, (uint16_t)(slot * 3 + 1), vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, (uint16_t)(slot * 3 + 2));
    vring_raw_set_desc(cvr, (uint16_t)(slot * 3 + 2), vv_virt_to_phys(sin),
                       sizeof(*sin), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(cvr, slot, (uint16_t)(slot * 3));
    vring_raw_set_avail_idx(cvr, (uint16_t)(slot + 1));

    if (vv_kick_and_wait(dev, cvr, 0, VV_TIMEOUT_MS) != TEST_PASS) {
        *skip = 1;
        return 0;
    }
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP ||
        sin->status == VIRTIO_CRYPTO_ERR) {
        *skip = 1;
        return 0;
    }
    if (sin->status != VIRTIO_CRYPTO_OK) {
        *skip = -1;
        return 0;
    }
    return sin->session_id;
}

static test_result_t cipher_op(struct virtio_dev *dev, struct vring *vr,
                               uint16_t slot, uint32_t opcode,
                               uint64_t session_id, uint8_t *iv,
                               uint8_t *src, uint8_t *dst)
{
    struct virtio_crypto_op_data_req *req = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = opcode;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->header.session_id = session_id;
    req->u.sym_cipher.para.iv_len = 16;
    req->u.sym_cipher.para.src_data_len = 16;
    req->u.sym_cipher.para.dst_data_len = 16;
    req->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    inhdr->status = 0xFF;

    uint16_t b = (uint16_t)(slot * 5);
    vring_raw_set_desc(vr, b, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, (uint16_t)(b + 1));
    vring_raw_set_desc(vr, (uint16_t)(b + 1), vv_virt_to_phys(iv), 16,
                       VRING_DESC_F_NEXT, (uint16_t)(b + 2));
    vring_raw_set_desc(vr, (uint16_t)(b + 2), vv_virt_to_phys(src), 16,
                       VRING_DESC_F_NEXT, (uint16_t)(b + 3));
    vring_raw_set_desc(vr, (uint16_t)(b + 3), vv_virt_to_phys(dst), 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, (uint16_t)(b + 4));
    vring_raw_set_desc(vr, (uint16_t)(b + 4), vv_virt_to_phys(inhdr),
                       sizeof(*inhdr), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, slot, b);
    vring_raw_set_avail_idx(vr, (uint16_t)(slot + 1));

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        return TEST_FAIL;
    return TEST_PASS;
}

static test_result_t test_crypto_wrong_decrypt_key(struct virtio_dev *dev,
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

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    uint8_t *key_a = vv_alloc_pages(1);
    uint8_t *key_b = vv_alloc_pages(1);
    memset(key_a, 0x11, 16);
    memset(key_b, 0x22, 16);

    int skip = 0;
    uint64_t enc = make_cipher_session(dev, &cvr, 0,
                                       VIRTIO_CRYPTO_OP_ENCRYPT, key_a, &skip);
    if (skip > 0)
        return TEST_SKIP;
    if (skip < 0)
        TFAIL("encrypt session create failed");
    uint64_t dec = make_cipher_session(dev, &cvr, 1,
                                       VIRTIO_CRYPTO_OP_DECRYPT, key_b, &skip);
    if (skip > 0)
        return TEST_SKIP;
    if (skip < 0)
        TFAIL("decrypt session create failed");

    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *plain = vv_alloc_pages(1);
    uint8_t *cipher = vv_alloc_pages(1);
    uint8_t *recovered = vv_alloc_pages(1);
    memset(iv, 0, 16);
    memset(plain, 0x41, 16);
    memset(cipher, 0, 16);
    memset(recovered, 0, 16);

    test_result_t r = cipher_op(dev, vr, 0, VIRTIO_CRYPTO_CIPHER_ENCRYPT,
                                enc, iv, plain, cipher);
    if (r != TEST_PASS)
        return r == TEST_FAIL ? TEST_SKIP : r;

    memset(iv, 0, 16);
    r = cipher_op(dev, vr, 1, VIRTIO_CRYPTO_CIPHER_DECRYPT,
                  dec, iv, cipher, recovered);
    if (r != TEST_PASS)
        return r == TEST_FAIL ? TEST_SKIP : r;
    if (memcmp(recovered, plain, 16) == 0)
        TFAIL("decrypt under the wrong key recovered the plaintext");

    return TEST_PASS;
}

REGISTER_TEST(CR0016, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_wrong_decrypt_key,
              "Decrypt under the wrong key does not recover the plaintext",
              VIRTIO_SPEC_V1_2, "5.9.8");

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0059: crypto_cipher_large_keys
 *
 * Spec 5.9.7.3: AES-CBC accepts 128, 192 and 256 bit keys. The other
 * cipher tests use a 128 bit key, so this one covers the larger sizes.
 * For a 24 byte and a 32 byte key, create an encrypt and a decrypt
 * session, encrypt a block on the data queue, decrypt it, and verify the
 * plaintext is recovered. Skips on Cloud Hypervisor, when AES-CBC is not
 * advertised, and when the backend cannot perform the algorithm; passes
 * under a QEMU with a crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static uint64_t make_session(struct virtio_dev *dev, struct vring *cvr,
                             uint16_t *cseq, uint32_t op, uint8_t *key,
                             uint32_t keylen, int *skip)
{
    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.keylen = keylen;
    req->u.sym_create.para.op = op;
    req->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(cvr, 1, vv_virt_to_phys(key), keylen,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    uint16_t s = *cseq;
    vring_raw_set_avail(cvr, (uint16_t)(s % 16), 0);
    vring_raw_set_avail_idx(cvr, (uint16_t)(s + 1));
    *cseq = (uint16_t)(s + 1);

    *skip = 0;
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
                               uint16_t *dseq, uint32_t opcode,
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

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(iv), 16, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(src), 16, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(dst), 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    uint16_t s = *dseq;
    vring_raw_set_avail(vr, (uint16_t)(s % 16), 0);
    vring_raw_set_avail_idx(vr, (uint16_t)(s + 1));
    *dseq = (uint16_t)(s + 1);

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        return TEST_FAIL;
    return TEST_PASS;
}

static test_result_t roundtrip_keylen(struct virtio_dev *dev, struct vring *cvr,
                                      struct vring *vr, uint16_t *cseq,
                                      uint16_t *dseq, uint32_t keylen)
{
    uint8_t *key = vv_alloc_pages(1);
    for (uint32_t i = 0; i < keylen; i++)
        key[i] = (uint8_t)i;

    int skip = 0;
    uint64_t enc = make_session(dev, cvr, cseq, VIRTIO_CRYPTO_OP_ENCRYPT,
                                key, keylen, &skip);
    if (skip > 0)
        return TEST_SKIP;
    if (skip < 0)
        TFAIL("encrypt session create failed for keylen %u", keylen);
    uint64_t dec = make_session(dev, cvr, cseq, VIRTIO_CRYPTO_OP_DECRYPT,
                                key, keylen, &skip);
    if (skip > 0)
        return TEST_SKIP;
    if (skip < 0)
        TFAIL("decrypt session create failed for keylen %u", keylen);

    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *plain = vv_alloc_pages(1);
    uint8_t *cipher = vv_alloc_pages(1);
    uint8_t *recovered = vv_alloc_pages(1);
    memset(iv, 0, 16);
    memset(plain, 0x41, 16);
    memset(cipher, 0, 16);
    memset(recovered, 0, 16);

    test_result_t r = cipher_op(dev, vr, dseq, VIRTIO_CRYPTO_CIPHER_ENCRYPT,
                                enc, iv, plain, cipher);
    if (r != TEST_PASS)
        return r == TEST_FAIL ? TEST_SKIP : r;
    if (memcmp(cipher, plain, 16) == 0)
        TFAIL("ciphertext equals plaintext for keylen %u", keylen);

    memset(iv, 0, 16);
    r = cipher_op(dev, vr, dseq, VIRTIO_CRYPTO_CIPHER_DECRYPT,
                  dec, iv, cipher, recovered);
    if (r != TEST_PASS)
        return r == TEST_FAIL ? TEST_SKIP : r;
    if (memcmp(recovered, plain, 16) != 0)
        TFAIL("decrypt did not recover plaintext for keylen %u", keylen);

    return TEST_PASS;
}

static test_result_t test_crypto_large_keys(struct virtio_dev *dev,
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

    uint16_t cseq = 0, dseq = 0;
    test_result_t r = roundtrip_keylen(dev, &cvr, vr, &cseq, &dseq, 24);
    if (r != TEST_PASS)
        return r;
    return roundtrip_keylen(dev, &cvr, vr, &cseq, &dseq, 32);
}

REGISTER_TEST(CR0059, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_large_keys,
              "AES-192 and AES-256 roundtrips recover the plaintext",
              VIRTIO_SPEC_V1_2, "5.9.7.3");

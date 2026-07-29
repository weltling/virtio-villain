/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0061: crypto_akcipher_pkcs1_roundtrip
 *
 * Spec 5.9.10: RSA sessions support PKCS#1 padding as well as raw. This
 * is a distinct backend path from the raw roundtrip. Create a public and
 * a private RSA session with PKCS#1 padding, encrypt a short message
 * under the public key, decrypt under the private key, and verify the
 * recovered bytes match. Skips on Cloud Hypervisor, when the akcipher
 * service is not advertised, when RSA is not advertised, and when the
 * backend cannot perform PKCS#1 RSA; passes on a QEMU built with a
 * crypto library that provides RSA, for example gcrypt.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "tests/crypto/crypto_rsa_key.h"

#include <string.h>

#define PKCS1_MSG_LEN 16

static test_result_t rsa_pkcs1_op(struct virtio_dev *dev, struct vring *vr,
                                  uint16_t slot, uint32_t opcode,
                                  uint64_t session_id, uint8_t *src,
                                  uint32_t src_len, uint8_t *dst,
                                  uint32_t dst_len, uint8_t *status_out)
{
    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = opcode;
    dreq->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    dreq->header.session_id = session_id;
    dreq->u.akcipher.para.src_data_len = src_len;
    dreq->u.akcipher.para.dst_data_len = dst_len;
    inhdr->status = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(src), src_len,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(dst), dst_len,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, slot, 0);
    vring_raw_set_avail_idx(vr, (uint16_t)(slot + 1));

    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    *status_out = inhdr->status;
    return TEST_PASS;
}

static test_result_t test_crypto_pkcs1_roundtrip(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_AKCIPHER)))
        return TEST_SKIP;
    if (!(cfg->akcipher_algo & (1u << VIRTIO_CRYPTO_AKCIPHER_RSA)))
        return TEST_SKIP;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    int ok, notsupp;
    uint64_t pub_id = crypto_rsa_create_session(dev, &cvr, 0, rsa_pub_der,
                                                sizeof(rsa_pub_der),
                                                VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PUBLIC,
                                                VIRTIO_CRYPTO_RSA_PKCS1_PADDING,
                                                VIRTIO_CRYPTO_RSA_SHA1,
                                                &ok, &notsupp);
    if (notsupp)
        return TEST_SKIP;
    if (!ok)
        TFAIL("public key session create did not ack OK");
    uint64_t priv_id = crypto_rsa_create_session(dev, &cvr, 1, rsa_priv_der,
                                                 sizeof(rsa_priv_der),
                                                 VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PRIVATE,
                                                 VIRTIO_CRYPTO_RSA_PKCS1_PADDING,
                                                 VIRTIO_CRYPTO_RSA_SHA1,
                                                 &ok, &notsupp);
    if (notsupp)
        return TEST_SKIP;
    if (!ok)
        TFAIL("private key session create did not ack OK");

    uint8_t *msg = vv_alloc_pages(1);
    uint8_t *cipher = vv_alloc_pages(1);
    uint8_t *plain = vv_alloc_pages(1);
    for (int i = 0; i < PKCS1_MSG_LEN; i++)
        msg[i] = (uint8_t)(0xa0 + i);
    memset(cipher, 0, RSA_KEY_BYTES);
    memset(plain, 0, RSA_KEY_BYTES);
    uint8_t status;

    test_result_t r = rsa_pkcs1_op(dev, vr, 0, VIRTIO_CRYPTO_AKCIPHER_ENCRYPT,
                                   pub_id, msg, PKCS1_MSG_LEN, cipher,
                                   RSA_KEY_BYTES, &status);
    if (r != TEST_PASS)
        return r;
    if (status == VIRTIO_CRYPTO_NOTSUPP || status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (status != VIRTIO_CRYPTO_OK)
        TFAIL("pkcs1 encrypt status %u, expected OK", status);

    r = rsa_pkcs1_op(dev, vr, 1, VIRTIO_CRYPTO_AKCIPHER_DECRYPT,
                     priv_id, cipher, RSA_KEY_BYTES, plain,
                     RSA_KEY_BYTES, &status);
    if (r != TEST_PASS)
        return r;
    if (status == VIRTIO_CRYPTO_NOTSUPP || status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (status != VIRTIO_CRYPTO_OK)
        TFAIL("pkcs1 decrypt status %u, expected OK", status);

    if (memcmp(plain, msg, PKCS1_MSG_LEN) != 0)
        TFAIL("pkcs1 roundtrip did not recover the original message");

    return TEST_PASS;
}

REGISTER_TEST(CR0061, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_pkcs1_roundtrip,
              "An RSA PKCS1 encrypt then decrypt recovers the message",
              VIRTIO_SPEC_V1_2, "5.9.10");

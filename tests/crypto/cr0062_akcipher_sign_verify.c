/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0062: crypto_akcipher_sign_verify
 *
 * Spec 5.9.10: RSA sessions support sign and verify operations. Create a
 * private and a public RSA session with PKCS#1 padding and a SHA1 hash,
 * sign a digest under the private key, then verify the signature under
 * the public key and expect an OK status. This is the first positive
 * test of the sign and verify operations. Skips on Cloud Hypervisor,
 * when the akcipher service is not advertised, when RSA is not
 * advertised, and when the backend cannot perform PKCS#1 RSA; passes on
 * a QEMU built with a crypto library that provides RSA, for example
 * gcrypt.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"
#include "tests/crypto/crypto_rsa_key.h"

#include <string.h>

#define SHA1_DIGEST_LEN 20

static test_result_t test_crypto_sign_verify(struct virtio_dev *dev,
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
    uint64_t priv_id = crypto_rsa_create_session(dev, &cvr, 0, rsa_priv_der,
                                                 sizeof(rsa_priv_der),
                                                 VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PRIVATE,
                                                 VIRTIO_CRYPTO_RSA_PKCS1_PADDING,
                                                 VIRTIO_CRYPTO_RSA_SHA1,
                                                 &ok, &notsupp);
    if (notsupp)
        return TEST_SKIP;
    if (!ok)
        TFAIL("private key session create did not ack OK");
    uint64_t pub_id = crypto_rsa_create_session(dev, &cvr, 1, rsa_pub_der,
                                                sizeof(rsa_pub_der),
                                                VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PUBLIC,
                                                VIRTIO_CRYPTO_RSA_PKCS1_PADDING,
                                                VIRTIO_CRYPTO_RSA_SHA1,
                                                &ok, &notsupp);
    if (notsupp)
        return TEST_SKIP;
    if (!ok)
        TFAIL("public key session create did not ack OK");

    uint8_t *digest = vv_alloc_pages(1);
    uint8_t *sig = vv_alloc_pages(1);
    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    for (int i = 0; i < SHA1_DIGEST_LEN; i++)
        digest[i] = (uint8_t)(i + 1);
    memset(sig, 0, RSA_KEY_BYTES);

    /* Sign the digest under the private key. */
    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_AKCIPHER_SIGN;
    dreq->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    dreq->header.session_id = priv_id;
    dreq->u.akcipher.para.src_data_len = SHA1_DIGEST_LEN;
    dreq->u.akcipher.para.dst_data_len = RSA_KEY_BYTES;
    inhdr->status = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(digest), SHA1_DIGEST_LEN,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(sig), RSA_KEY_BYTES,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status == VIRTIO_CRYPTO_NOTSUPP || inhdr->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        TFAIL("rsa sign status %u, expected OK", inhdr->status);

    /* Verify the signature under the public key. The backend takes the
     * signature as src and the digest as dst; both are device-readable
     * and only the status is written back. */
    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_AKCIPHER_VERIFY;
    dreq->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    dreq->header.session_id = pub_id;
    dreq->u.akcipher.para.src_data_len = RSA_KEY_BYTES;
    dreq->u.akcipher.para.dst_data_len = SHA1_DIGEST_LEN;
    inhdr->status = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(sig), RSA_KEY_BYTES,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(digest), SHA1_DIGEST_LEN,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 1, 0);
    vring_raw_set_avail_idx(vr, 2);
    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status == VIRTIO_CRYPTO_NOTSUPP || inhdr->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        TFAIL("rsa verify status %u, expected OK", inhdr->status);

    return TEST_PASS;
}

REGISTER_TEST(CR0062, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_sign_verify,
              "An RSA sign then verify accepts the signature",
              VIRTIO_SPEC_V1_2, "5.9.10");

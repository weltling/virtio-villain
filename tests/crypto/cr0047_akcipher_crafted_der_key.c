/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0047: crypto_akcipher_crafted_der_key
 *
 * Fault injection. Spec 5.9.10 hands the akcipher key blob to the device
 * for parsing. The device decodes the DER structure to extract the RSA
 * modulus and exponent. A blob whose outer DER length is self consistent
 * but whose contents are garbage drives the DER decoder deeper than a
 * blob it rejects on sight, so it probes the decoder for a short read or
 * an overrun. Post an RSA create session with a crafted DER prefix over
 * a matching key length and verify the host survives. The device outcome
 * is up to it: PASS, REJECT and WEDGED are all acceptable as long as the
 * next test can still run. Most valuable under an ASan VMM with akcipher
 * enabled. Skips on Cloud Hypervisor, when the akcipher service is not
 * advertised, and when RSA is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define DER_KEY_LEN 128

static test_result_t test_crypto_akcipher_crafted_der(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    (void)vr;
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

    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_AKCIPHER_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    req->u.akcipher_create.para.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    req->u.akcipher_create.para.keytype = VIRTIO_CRYPTO_AKCIPHER_KEY_TYPE_PUBLIC;
    req->u.akcipher_create.para.keylen = DER_KEY_LEN;
    req->u.akcipher_create.para.rsa.padding_algo = VIRTIO_CRYPTO_RSA_RAW_PADDING;
    req->u.akcipher_create.para.rsa.hash_algo = VIRTIO_CRYPTO_RSA_NO_HASH;

    /* SEQUENCE (0x30), long form length of 125 bytes, then garbage. The
     * outer length spans the rest of the blob so the decoder walks in
     * before it hits an unexpected inner tag. */
    memset(key, 0x41, DER_KEY_LEN);
    key[0] = 0x30;
    key[1] = 0x81;
    key[2] = (uint8_t)(DER_KEY_LEN - 3);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), DER_KEY_LEN,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a crafted DER akcipher key");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0047, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_akcipher_crafted_der,
              "A crafted DER akcipher key keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.10");

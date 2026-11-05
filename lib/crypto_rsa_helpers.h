/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_CRYPTO_RSA_HELPERS_H
#define VV_CRYPTO_RSA_HELPERS_H

#include "virtio_test.h"

/*
 * Create an RSA akcipher session on the control queue cvr. Returns the
 * session id and sets *ok when the device acked OK, or *notsupp when the
 * backend cannot perform the operation. Uses control descriptors 0..2.
 */
static inline uint64_t crypto_rsa_create_session(struct virtio_dev *dev,
                                                 struct vring *cvr,
                                                 uint16_t slot,
                                                 const uint8_t *der,
                                                 uint32_t derlen,
                                                 uint32_t keytype,
                                                 uint32_t padding,
                                                 uint32_t hash,
                                                 int *ok, int *notsupp)
{
    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    req->header.opcode = VIRTIO_CRYPTO_AKCIPHER_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    req->u.akcipher_create.para.algo = VIRTIO_CRYPTO_AKCIPHER_RSA;
    req->u.akcipher_create.para.keytype = keytype;
    req->u.akcipher_create.para.keylen = derlen;
    req->u.akcipher_create.para.rsa.padding_algo = padding;
    req->u.akcipher_create.para.rsa.hash_algo = hash;
    memcpy(key, der, derlen);
    sin->status = 0xFF;

    vv_desc(cvr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(cvr, 1, key, derlen, VRING_DESC_F_NEXT, 2);
    vv_desc(cvr, 2, sin, sizeof(*sin), VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(cvr, slot, 0);
    vring_raw_set_avail_idx(cvr, (uint16_t)(slot + 1));

    *ok = 0;
    *notsupp = 0;
    if (vv_kick_and_wait(dev, cvr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return 0;
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP ||
        sin->status == VIRTIO_CRYPTO_ERR) {
        *notsupp = 1;
        return 0;
    }
    if (sin->status == VIRTIO_CRYPTO_OK)
        *ok = 1;
    return sin->session_id;
}

#endif /* VV_CRYPTO_RSA_HELPERS_H */

/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0014: crypto_create_bad_keylen
 *
 * Spec 5.9.7: a create session request whose key length is not valid
 * for the named algorithm must be rejected. AES accepts 16, 24 or 32
 * byte keys, so request an AES-CBC session with a 7 byte key and verify
 * the device responds with a non OK status rather than accepting it.
 * Skips on Cloud Hypervisor and when the cipher service is not offered;
 * passes under QEMU.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define BAD_KEYLEN 7u

static test_result_t test_crypto_create_bad_keylen(struct virtio_dev *dev,
                                                   struct vring *vr)
{
    (void)vr;
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

    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    req->u.sym_create.para.keylen = BAD_KEYLEN;
    req->u.sym_create.para.op = VIRTIO_CRYPTO_OP_ENCRYPT;
    req->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(key, 0, BAD_KEYLEN);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), BAD_KEYLEN,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    if (vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (sin->status == VIRTIO_CRYPTO_OK)
        TFAIL("device accepted a session with an invalid key length");

    return TEST_PASS;
}

REGISTER_TEST(CR0014, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_create_bad_keylen,
              "Create session with an invalid key length is rejected",
              VIRTIO_SPEC_V1_2, "5.9.7");

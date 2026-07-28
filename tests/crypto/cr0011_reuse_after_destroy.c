/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0011: crypto_cipher_reuse_after_destroy
 *
 * Spec 5.9.8: once a session is destroyed its id is no longer valid, so
 * a data queue operation that names the destroyed session must be
 * rejected with a non OK status rather than processed. Create an
 * AES-CBC encrypt session, destroy it on the control queue, then submit
 * an encrypt request on a data queue with the now stale session id and
 * verify the device responds with a non OK status. Skips on Cloud
 * Hypervisor, when the cipher service or AES-CBC is not advertised, and
 * when the backend cannot create the session; passes under a QEMU with
 * a crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_reuse_after_destroy(struct virtio_dev *dev,
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
    memset(key, 0x2b, 16);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(creq), sizeof(*creq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP ||
        sin->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (sin->status != VIRTIO_CRYPTO_OK)
        TFAIL("create session status %u, expected OK", sin->status);

    uint64_t session_id = sin->session_id;

    /* Destroy the session before using it. */
    struct virtio_crypto_inhdr *dinhdr = vv_alloc_pages(1);
    memset(creq, 0, sizeof(*creq));
    creq->header.opcode = VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION;
    creq->u.destroy.session_id = session_id;
    dinhdr->status = 0xFF;
    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(creq), sizeof(*creq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(dinhdr), sizeof(*dinhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 1, 0);
    vring_raw_set_avail_idx(&cvr, 2);
    r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (dinhdr->status != VIRTIO_CRYPTO_OK)
        TFAIL("destroy status %u, expected OK", dinhdr->status);

    /* Encrypt using the stale session id, which must be rejected. */
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
    memset(iv, 0, 16);
    memset(src, 0x41, 16);
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
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    r = vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
    if (r != TEST_PASS)
        return r;
    if (inhdr->status == VIRTIO_CRYPTO_OK)
        TFAIL("encrypt on a destroyed session was accepted");

    return TEST_PASS;
}

REGISTER_TEST(CR0011, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_reuse_after_destroy,
              "Cipher op on a destroyed session is rejected",
              VIRTIO_SPEC_V1_2, "5.9.8");

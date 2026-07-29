/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0060: crypto_cipher_known_answer
 *
 * Spec 5.9.8: an AES-CBC encrypt must produce the exact ciphertext
 * defined by the algorithm. The other cipher tests check determinism and
 * roundtrip but never a fixed vector, so a backend that encrypts into
 * garbage or the wrong mode would pass them. Encrypt a known plaintext
 * with a known key and IV and compare against the precomputed
 * ciphertext. Skips on Cloud Hypervisor, when AES-CBC is not advertised,
 * and when the backend cannot perform the algorithm; passes under a QEMU
 * with a crypto library.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

/* AES-128-CBC known answer vector (single block, no padding). */
static const uint8_t kat_key[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};
static const uint8_t kat_iv[16] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};
static const uint8_t kat_pt[16] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
};
static const uint8_t kat_ct[16] = {
    0x1e, 0xca, 0x87, 0x0f, 0xfe, 0xa1, 0x14, 0xb7,
    0xfd, 0x6c, 0xf3, 0x63, 0xc3, 0x0b, 0x96, 0xb1
};

static test_result_t test_crypto_known_answer(struct virtio_dev *dev,
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
    memcpy(key, kat_key, 16);
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
    if (vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (sin->status == VIRTIO_CRYPTO_NOTSUPP || sin->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (sin->status != VIRTIO_CRYPTO_OK)
        TFAIL("cipher session create status %u, expected OK", sin->status);
    uint64_t session_id = sin->session_id;

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
    memcpy(iv, kat_iv, 16);
    memcpy(src, kat_pt, 16);
    memset(dst, 0, 16);
    inhdr->status = 0xFF;
    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(iv), 16, VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(src), 16, VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, vv_virt_to_phys(dst), 16,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);
    if (vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS) != TEST_PASS)
        return TEST_REJECT;
    if (inhdr->status == VIRTIO_CRYPTO_NOTSUPP || inhdr->status == VIRTIO_CRYPTO_ERR)
        return TEST_SKIP;
    if (inhdr->status != VIRTIO_CRYPTO_OK)
        TFAIL("encrypt status %u, expected OK", inhdr->status);

    if (memcmp(dst, kat_ct, 16) != 0)
        TFAIL("ciphertext does not match the known answer vector");

    return TEST_PASS;
}

REGISTER_TEST(CR0060, VIRTIO_PCI_DEVICE_CRYPTO, test_crypto_known_answer,
              "AES-CBC encrypt matches a known answer vector",
              VIRTIO_SPEC_V1_2, "5.9.8");

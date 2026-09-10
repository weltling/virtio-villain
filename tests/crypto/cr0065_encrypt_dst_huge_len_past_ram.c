/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0065: crypto encrypt dst buffer len past end of guest RAM.
 *
 * Same shape as B0162 for the virtio-crypto data path: submit a cipher
 * encrypt whose writable destination descriptor base lives in valid
 * guest RAM but whose length crosses the end of all System RAM. The
 * device must not write ciphertext outside the guest's mapping or crash
 * the VMM. Distinct from CR0019, which overflows the src_data_len field
 * over a small descriptor; here the descriptor length itself runs past
 * RAM. Skips on Cloud Hypervisor and when the cipher service is not
 * advertised.
 *
 * Spec 5.9.8.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_encrypt_dst_huge_len_past_ram(
    struct virtio_dev *dev,
    struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    uint64_t ram_top = vv_parse_ram_top();
    if (ram_top == 0)
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    uint64_t dst_phys;
    if (!vv_alloc_page_near_ram_top(ram_top, &dst_phys))
        return TEST_SKIP;

    uint64_t overshoot = (ram_top - dst_phys) + (1ULL << 30);
    if (overshoot > 0xFFFFFFFFULL)
        overshoot = 0xFFFFFFFFULL;
    uint32_t len = (uint32_t)overshoot;

    memset(dreq, 0, sizeof(*dreq));
    dreq->header.opcode = VIRTIO_CRYPTO_CIPHER_ENCRYPT;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = len;
    dreq->u.sym_cipher.para.dst_data_len = len;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(iv, 0, 16);
    memset(src, 0x41, 16);
    inhdr->status = 0xFF;

    vring_raw_set_desc(vr, 0, vv_virt_to_phys(dreq), sizeof(*dreq),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(vr, 1, vv_virt_to_phys(iv), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(vr, 2, vv_virt_to_phys(src), 16,
                       VRING_DESC_F_NEXT, 3);
    vring_raw_set_desc(vr, 3, dst_phys, len,
                       VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vring_raw_set_desc(vr, 4, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(vr, 0, 0);
    vring_raw_set_avail_idx(vr, 1);

    return vv_kick_and_wait(dev, vr, 0, VV_TIMEOUT_MS);
}

REGISTER_TEST(CR0065, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_encrypt_dst_huge_len_past_ram,
              "Crypto encrypt dst buffer len crosses end of RAM",
              VIRTIO_SPEC_V1_2, "5.9.8");

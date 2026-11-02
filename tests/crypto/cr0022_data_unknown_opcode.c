/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0022: crypto_data_unknown_opcode
 *
 * Fault injection. Spec 5.9.8 defines the data queue opcodes. A data
 * request naming an opcode the device does not implement must be
 * rejected without crashing the host. Post a data request with a
 * garbage opcode and verify the host survives. The device outcome is up
 * to it: PASS, REJECT and WEDGED are all acceptable as long as the next
 * test can still run. Most valuable under an ASan VMM. Skips on Cloud
 * Hypervisor and when the cipher service is not advertised.
 */
#include "lib/virtio_test.h"

#define BOGUS_DATA_OPCODE 0xbeefu

static test_result_t test_crypto_data_unknown_opcode(struct virtio_dev *dev,
                                                     struct vring *vr)
{
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct virtio_crypto_op_data_req *dreq = vv_alloc_pages(1);
    uint8_t *iv = vv_alloc_pages(1);
    uint8_t *src = vv_alloc_pages(1);
    uint8_t *dst = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);

    dreq->header.opcode = BOGUS_DATA_OPCODE;
    dreq->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
    dreq->header.session_id = 0;
    dreq->u.sym_cipher.para.iv_len = 16;
    dreq->u.sym_cipher.para.src_data_len = 16;
    dreq->u.sym_cipher.para.dst_data_len = 16;
    dreq->u.sym_cipher.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(src, 0x41, 16);
    inhdr->status = 0xFF;

    vv_desc(vr, 0, dreq, sizeof(*dreq), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, iv, 16, VRING_DESC_F_NEXT, 2);
    vv_desc(vr, 2, src, 16, VRING_DESC_F_NEXT, 3);
    vv_desc(vr, 3, dst, 16, VRING_DESC_F_NEXT | VRING_DESC_F_WRITE, 4);
    vv_desc(vr, 4, inhdr, sizeof(*inhdr), VRING_DESC_F_WRITE, 0);
    test_result_t r = vv_submit_and_wait(dev, vr, 0);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on an unknown data opcode");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0022, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_data_unknown_opcode,
              "An unknown data opcode keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.8");

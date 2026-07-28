/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0025: crypto_create_keylen_over_max
 *
 * Fault injection. Spec 5.9.4 advertises max_cipher_key_len and spec
 * 5.9.7 carries the key length in the create session request. A request
 * whose key length exceeds the advertised maximum, backed by a matching
 * oversized key descriptor, must be bounded by the device rather than
 * accepted blindly, and must not crash the host. Post a create with
 * keylen set past max_cipher_key_len over a key buffer of that size and
 * verify the host survives. The device outcome is up to it: PASS, REJECT
 * and WEDGED are all acceptable as long as the next test can still run.
 * Most valuable under an ASan VMM. Skips on Cloud Hypervisor and when
 * the cipher service is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_keylen_over_max(struct virtio_dev *dev,
                                                 struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    uint32_t maxlen = cfg->max_cipher_key_len;
    uint32_t keylen = maxlen + 32u;
    /* Keep the key within a single page so the descriptor can match. */
    if (keylen == 0 || keylen > 4096u)
        keylen = 96u;

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
    req->u.sym_create.para.keylen = keylen;
    req->u.sym_create.para.op = VIRTIO_CRYPTO_OP_ENCRYPT;
    req->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
    memset(key, 0x2b, keylen);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), keylen,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on a key length past the maximum");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0025, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_keylen_over_max,
              "Create with a key length past the maximum keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.7");

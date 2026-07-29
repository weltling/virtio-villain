/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0058: crypto_session_create_destroy_churn
 *
 * Fault injection. Spec 5.9.7 lets the driver create and destroy cipher
 * sessions. Rapidly creating a session and destroying it many times in a
 * row stresses the device session table and its id reuse. Run many
 * create then destroy cycles and verify the host keeps servicing the
 * control queue throughout. The final outcome is up to the device, PASS,
 * REJECT and WEDGED are all acceptable as long as the run continues.
 * Most valuable under an ASan VMM. Skips on Cloud Hypervisor and when
 * the cipher service is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

#define CRYPTO_CHURN_CYCLES 64

static test_result_t test_crypto_create_destroy_churn(struct virtio_dev *dev,
                                                      struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_CIPHER)))
        return TEST_SKIP;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    struct virtio_crypto_inhdr *inhdr = vv_alloc_pages(1);
    memset(key, 0x2b, 16);
    uint16_t idx = 0;

    for (int i = 0; i < CRYPTO_CHURN_CYCLES; i++) {
        /* Create a cipher session. */
        memset(req, 0, sizeof(*req));
        req->header.opcode = VIRTIO_CRYPTO_CIPHER_CREATE_SESSION;
        req->header.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
        req->u.sym_create.para.algo = VIRTIO_CRYPTO_CIPHER_AES_CBC;
        req->u.sym_create.para.keylen = 16;
        req->u.sym_create.para.op = VIRTIO_CRYPTO_OP_ENCRYPT;
        req->u.sym_create.op_type = VIRTIO_CRYPTO_SYM_OP_CIPHER;
        memset(sin, 0, sizeof(*sin));
        sin->status = 0xFF;
        vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 1);
        vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), 16,
                           VRING_DESC_F_NEXT, 2);
        vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(&cvr, (uint16_t)(idx % 16), 0);
        vring_raw_set_avail_idx(&cvr, (uint16_t)(idx + 1));
        idx++;
        test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
        if (r == TEST_FAIL)
            TFAIL("device reported failure creating session %d", i);
        if (r == TEST_WEDGED)
            return TEST_WEDGED;
        if (r != TEST_PASS)
            break;
        if (sin->status != VIRTIO_CRYPTO_OK)
            break;

        /* Destroy it. */
        memset(req, 0, sizeof(*req));
        req->header.opcode = VIRTIO_CRYPTO_CIPHER_DESTROY_SESSION;
        req->u.destroy.session_id = sin->session_id;
        inhdr->status = 0xFF;
        vring_raw_set_desc(&cvr, 3, vv_virt_to_phys(req), sizeof(*req),
                           VRING_DESC_F_NEXT, 4);
        vring_raw_set_desc(&cvr, 4, vv_virt_to_phys(inhdr), sizeof(*inhdr),
                           VRING_DESC_F_WRITE, 0);
        vring_raw_set_avail(&cvr, (uint16_t)(idx % 16), 3);
        vring_raw_set_avail_idx(&cvr, (uint16_t)(idx + 1));
        idx++;
        r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
        if (r == TEST_FAIL)
            TFAIL("device reported failure destroying session %d", i);
        if (r == TEST_WEDGED)
            return TEST_WEDGED;
        if (r != TEST_PASS)
            break;
    }

    return TEST_PASS;
}

REGISTER_TEST(CR0058, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_create_destroy_churn,
              "Repeated session create and destroy keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.7");

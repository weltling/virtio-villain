/* SPDX-License-Identifier: Apache-2.0 */
/*
 * CR0041: crypto_mac_authkeylen_overflow
 *
 * Fault injection. Spec 5.9.9.2 carries the authenticated key length in
 * the MAC create session request, and the device gathers that many key
 * bytes from the request. A key length of 0xffffffff over a small key
 * descriptor must not drive the device to read past the buffer or crash
 * the host. Post a MAC create session with an enormous auth key length
 * over a 16 byte key and verify the host survives. The device outcome is
 * up to it: PASS, REJECT and WEDGED are all acceptable as long as the
 * next test can still run. Most valuable under an ASan VMM. Skips on
 * Cloud Hypervisor and when the MAC service is not advertised.
 */
#include "tests/test.h"
#include "lib/util.h"
#include "lib/vring.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_spec.h"

#include <string.h>

static test_result_t test_crypto_mac_authkeylen_overflow(struct virtio_dev *dev,
                                                         struct vring *vr)
{
    (void)vr;
    if (!dev->device_cfg)
        return TEST_SKIP;
    volatile struct virtio_crypto_config *cfg =
        (volatile struct virtio_crypto_config *)dev->device_cfg;
    if (!(cfg->crypto_services & (1u << VIRTIO_CRYPTO_SERVICE_MAC)))
        return TEST_SKIP;

    struct vring cvr;
    vring_alloc(&cvr, 16);
    vring_attach(dev, &cvr, (uint16_t)cfg->max_dataqueues);

    struct virtio_crypto_op_ctrl_req *req = vv_alloc_pages(1);
    uint8_t *key = vv_alloc_pages(1);
    struct virtio_crypto_session_input *sin = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    req->header.opcode = VIRTIO_CRYPTO_MAC_CREATE_SESSION;
    req->header.algo = VIRTIO_CRYPTO_MAC_HMAC_SHA1;
    req->u.mac_create.para.algo = VIRTIO_CRYPTO_MAC_HMAC_SHA1;
    req->u.mac_create.para.hash_result_len = 20;
    req->u.mac_create.para.auth_key_len = 0xffffffffu;
    memset(key, 0x2b, 16);
    memset(sin, 0, sizeof(*sin));
    sin->status = 0xFF;

    vring_raw_set_desc(&cvr, 0, vv_virt_to_phys(req), sizeof(*req),
                       VRING_DESC_F_NEXT, 1);
    vring_raw_set_desc(&cvr, 1, vv_virt_to_phys(key), 16,
                       VRING_DESC_F_NEXT, 2);
    vring_raw_set_desc(&cvr, 2, vv_virt_to_phys(sin), sizeof(*sin),
                       VRING_DESC_F_WRITE, 0);
    vring_raw_set_avail(&cvr, 0, 0);
    vring_raw_set_avail_idx(&cvr, 1);

    test_result_t r = vv_kick_and_wait(dev, &cvr, 0, VV_TIMEOUT_MS);
    if (r == TEST_FAIL)
        TFAIL("device reported failure on an oversized MAC key length");
    if (r == TEST_PASS || r == TEST_WEDGED)
        return r;
    TREJECT("no device response within timeout");
}

REGISTER_TEST(CR0041, VIRTIO_PCI_DEVICE_CRYPTO,
              test_crypto_mac_authkeylen_overflow,
              "An oversized MAC key length keeps the host alive",
              VIRTIO_SPEC_V1_2, "5.9.9.2");

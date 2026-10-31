/* Spec 5.15.6.1 requires ERROR for PLUG outside the usable region. */
#include "tests/test.h"

static test_result_t test_mem_plug_beyond_usable(struct virtio_dev *dev,
                                               struct vring *vr)
{
    if (!dev->device_cfg ||
        dev->device_cfg_length < sizeof(struct virtio_mem_config))
        TSKIP("memory device configuration is unavailable");

    volatile struct virtio_mem_config *cfg =
        (volatile struct virtio_mem_config *)dev->device_cfg;
    uint64_t block_size = cfg->block_size;
    uint64_t region_addr = cfg->addr;
    uint64_t region_size = cfg->region_size;
    uint64_t usable_size = cfg->usable_region_size;
    uint64_t plugged_before = cfg->plugged_size;

    if (block_size == 0 || region_addr % block_size != 0 ||
        usable_size % block_size != 0)
        TSKIP("memory configuration has no aligned usable boundary");
    if (usable_size >= region_size || region_size - usable_size < block_size)
        TSKIP("no whole block outside usable_region_size");
    if (region_addr > UINT64_MAX - region_size)
        TSKIP("memory region wraps the address space");

    struct virtio_mem_req *req = vv_alloc_pages(1);
    struct virtio_mem_resp *resp = vv_alloc_pages(1);
    memset(req, 0, sizeof(*req));
    memset(resp, 0xff, sizeof(*resp));
    req->type = VIRTIO_MEM_REQ_PLUG;
    req->addr = region_addr + usable_size;
    req->nb_blocks = 1;

    uint16_t used_idx = vr->used->idx;
    vv_desc(vr, 0, req, sizeof(*req), VRING_DESC_F_NEXT, 1);
    vv_desc(vr, 1, resp, sizeof(*resp), VRING_DESC_F_WRITE, 0);
    test_result_t result = vv_submit_and_wait(dev, vr, 0);
    if (result == TEST_REJECT)
        TFAIL("PLUG outside usable_region_size received no response");
    if (result != TEST_PASS)
        return result;

    if (cfg->usable_region_size != usable_size)
        TSKIP("usable_region_size changed during the request");

    uint32_t used_len = vr->used->ring[used_idx % vr->size].len;
    if (used_len != sizeof(*resp))
        TFAIL("PLUG response length %u, expected %zu", used_len, sizeof(*resp));
    if (cfg->plugged_size != plugged_before)
        TFAIL("PLUG outside usable_region_size changed plugged_size");
    if (resp->type == VIRTIO_MEM_RESP_BUSY)
        TSKIP("memory device is busy");
    if (resp->type != VIRTIO_MEM_RESP_ERROR)
        TFAIL("PLUG outside usable_region_size returned %u, expected ERROR",
              resp->type);

    return TEST_PASS;
}

REGISTER_TEST(R0045, VIRTIO_PCI_DEVICE_MEM, test_mem_plug_beyond_usable,
              "PLUG beyond usable_region_size returns ERROR without plugging",
              VIRTIO_SPEC_V1_4, "5.15.6.1");

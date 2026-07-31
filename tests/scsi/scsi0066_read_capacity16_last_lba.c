/* SPDX-License-Identifier: Apache-2.0 */
/*
 * SCSI0066: read_capacity16_last_lba
 *
 * READ CAPACITY(16) reports the last logical block address of 32767 for
 * the 16 MiB backing image.
 */
#include "tests/scsi/scsi_util.h"

static test_result_t test_scsi_rc16_last_lba(struct virtio_dev *dev,
                                             struct vring *vr)
{
    struct virtio_scsi_cmd_req *req = vv_alloc_pages(1);
    struct virtio_scsi_cmd_resp *resp = vv_alloc_pages(1);
    uint8_t *data = vv_alloc_pages(1);

    uint16_t seq = scsi_clear_ua(dev, vr, 0);

    memset(req, 0, sizeof(*req));
    scsi_set_lun(req->lun, 0, 0);
    req->cdb[0] = 0x9E;      /* SERVICE ACTION IN(16) */
    req->cdb[1] = 0x10;      /* READ CAPACITY(16) */
    req->cdb[13] = 32;

    test_result_t r = scsi_do_cmd(dev, vr, req, resp, data, 32,
                                  SCSI_DATA_IN, seq);
    if (r != TEST_PASS)
        return r;
    __sync_synchronize();
    if (resp->status != 0)
        TFAIL("status 0x%02x", resp->status);
    /* Last LBA is a 64-bit field; the value fits in the low 32 bits. */
    if (scsi_be32(data) != 0)
        TFAIL("high last LBA word 0x%08x", scsi_be32(data));
    uint32_t last_lba = scsi_be32(data + 4);
    if (last_lba != 32767)
        TFAIL("last_lba %u, expected 32767", last_lba);
    return TEST_PASS;
}

REGISTER_TEST_Q(SCSI0066, VIRTIO_PCI_DEVICE_SCSI, test_scsi_rc16_last_lba,
                "READ CAPACITY(16) reports the expected last LBA",
                VIRTIO_SPEC_V1_4, "5.6.6.1", VV_QUEUE_LAST);

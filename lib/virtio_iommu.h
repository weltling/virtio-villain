/* SPDX-License-Identifier: Apache-2.0 */
#ifndef VV_VIRTIO_IOMMU_H
#define VV_VIRTIO_IOMMU_H

#include <stdint.h>

/*
 * virtio-iommu protocol structures, as defined in the virtio v1.2 spec
 * chapter 5.13. Only the subset needed by the test harness.
 */

/* Feature bits */
#define VIRTIO_IOMMU_F_INPUT_RANGE    0
#define VIRTIO_IOMMU_F_DOMAIN_RANGE   1
#define VIRTIO_IOMMU_F_MAP_UNMAP      2
#define VIRTIO_IOMMU_F_BYPASS         3
#define VIRTIO_IOMMU_F_PROBE          4
#define VIRTIO_IOMMU_F_MMIO           5
#define VIRTIO_IOMMU_F_BYPASS_CONFIG  6

/* Request types */
#define VIRTIO_IOMMU_T_ATTACH         1
#define VIRTIO_IOMMU_T_DETACH         2
#define VIRTIO_IOMMU_T_MAP            3
#define VIRTIO_IOMMU_T_UNMAP          4
#define VIRTIO_IOMMU_T_PROBE          5

/* Status codes */
#define VIRTIO_IOMMU_S_OK             0
#define VIRTIO_IOMMU_S_IOERR          1
#define VIRTIO_IOMMU_S_UNSUPP         2
#define VIRTIO_IOMMU_S_DEVERR         3
#define VIRTIO_IOMMU_S_INVAL          4
#define VIRTIO_IOMMU_S_RANGE          5
#define VIRTIO_IOMMU_S_NOENT          6
#define VIRTIO_IOMMU_S_FAULT          7
#define VIRTIO_IOMMU_S_NOMEM          8

/* MAP flags */
#define VIRTIO_IOMMU_MAP_F_READ       (1 << 0)
#define VIRTIO_IOMMU_MAP_F_WRITE      (1 << 1)
#define VIRTIO_IOMMU_MAP_F_MMIO       (1 << 2)

/* ATTACH flags */
#define VIRTIO_IOMMU_ATTACH_F_BYPASS  (1 << 0)

struct virtio_iommu_req_head {
    uint8_t  type;
    uint8_t  reserved[3];
} __attribute__((packed));

struct virtio_iommu_req_tail {
    uint8_t  status;
    uint8_t  reserved[3];
} __attribute__((packed));

struct virtio_iommu_req_attach {
    struct virtio_iommu_req_head head;
    uint32_t domain;
    uint32_t endpoint;
    uint32_t flags;
    uint8_t  reserved[4];
    struct virtio_iommu_req_tail tail;
} __attribute__((packed));

struct virtio_iommu_req_detach {
    struct virtio_iommu_req_head head;
    uint32_t domain;
    uint32_t endpoint;
    uint8_t  reserved[8];
    struct virtio_iommu_req_tail tail;
} __attribute__((packed));

struct virtio_iommu_req_map {
    struct virtio_iommu_req_head head;
    uint32_t domain;
    uint64_t virt_start;
    uint64_t virt_end;
    uint64_t phys_start;
    uint32_t flags;
    struct virtio_iommu_req_tail tail;
} __attribute__((packed));

struct virtio_iommu_req_unmap {
    struct virtio_iommu_req_head head;
    uint32_t domain;
    uint64_t virt_start;
    uint64_t virt_end;
    uint8_t  reserved[4];
    struct virtio_iommu_req_tail tail;
} __attribute__((packed));

struct virtio_iommu_req_probe {
    struct virtio_iommu_req_head head;
    uint32_t endpoint;
    uint8_t  reserved[64];
    uint8_t  properties[64];
    struct virtio_iommu_req_tail tail;
} __attribute__((packed));

/* Truncated device config view used by capability boundary tests. */
struct iommu_config {
    uint32_t page_size_mask;
    struct { uint64_t start, end; } input_range;
    struct { uint32_t start, end; } domain_range;
} __attribute__((packed));

/* Full device configuration layout (spec 5.13.4). */
struct virtio_iommu_config {
    uint32_t page_size_mask;
    struct {
        uint64_t start;
        uint64_t end;
    } input_range;
    struct {
        uint32_t start;
        uint32_t end;
    } domain_range;
    uint32_t probe_size;
    uint8_t  bypass;
    uint8_t  reserved[3];
} __attribute__((packed));

#endif

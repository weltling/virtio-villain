/* SPDX-License-Identifier: Apache-2.0 */
/*
 * fuse.h - Shared FUSE wire constants and structs for virtio-fs tests.
 *
 * virtio-fs (virtio spec 5.11) carries the FUSE protocol over the
 * request virtqueue. The opcodes and request structs below were copied
 * into every fs test, which let the values drift apart. Tests MUST
 * include this header instead of redefining them locally.
 *
 * Values are anchored to the Linux FUSE UAPI (include/uapi/linux/fuse.h).
 */
#ifndef VV_FUSE_H
#define VV_FUSE_H

#include <stdint.h>

/* FUSE opcodes (linux/fuse.h enum fuse_opcode). */
#define FUSE_LOOKUP        1
#define FUSE_FORGET        2
#define FUSE_GETATTR       3
#define FUSE_OPEN          14
#define FUSE_READ          15
#define FUSE_WRITE         16
#define FUSE_INIT          26
#define FUSE_READDIR       28
#define FUSE_INTERRUPT     36
#define FUSE_NOTIFY_REPLY  41
#define FUSE_BATCH_FORGET  42

/* Header prepended to every FUSE request. */
struct fuse_in_header {
    uint32_t len;
    uint32_t opcode;
    uint64_t unique;
    uint64_t nodeid;
    uint32_t uid;
    uint32_t gid;
    uint32_t pid;
    uint32_t padding;
} __attribute__((packed));

/* Alternate name used by some tests for the same request header. */
struct fuse_in_header_min {
    uint32_t len;
    uint32_t opcode;
    uint64_t unique;
    uint64_t nodeid;
    uint32_t uid;
    uint32_t gid;
    uint32_t pid;
    uint32_t padding;
} __attribute__((packed));

/* FUSE_INIT request body. */
struct fuse_init_in {
    uint32_t major;
    uint32_t minor;
    uint32_t max_readahead;
    uint32_t flags;
} __attribute__((packed));

/* FUSE_GETATTR request body. */
struct fuse_getattr_in {
    uint32_t getattr_flags;
    uint32_t dummy;
    uint64_t fh;
} __attribute__((packed));

/* FUSE_INTERRUPT request body. */
struct fuse_interrupt_in {
    uint64_t unique;
} __attribute__((packed));

/* FUSE_READ request body. */
struct fuse_read_in {
    uint64_t fh;
    uint64_t offset;
    uint32_t size;
    uint32_t read_flags;
    uint64_t lock_owner;
    uint32_t flags;
    uint32_t padding;
} __attribute__((packed));

/* FUSE_BATCH_FORGET request body. */
struct fuse_batch_forget_in {
    uint32_t count;
    uint32_t dummy;
} __attribute__((packed));

/* FUSE_WRITE request body. */
struct fuse_write_in {
    uint64_t fh;
    uint64_t offset;
    uint32_t size;
    uint32_t write_flags;
    uint64_t lock_owner;
    uint32_t flags;
    uint32_t padding;
} __attribute__((packed));

/* FUSE_FORGET request body. */
struct fuse_forget_in {
    uint64_t nlookup;
} __attribute__((packed));

/* FUSE_OPEN request body. */
struct fuse_open_in {
    uint32_t flags;
    uint32_t unused;
} __attribute__((packed));

#endif /* VV_FUSE_H */

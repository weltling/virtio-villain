/* SPDX-License-Identifier: Apache-2.0 */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#if defined(__x86_64__) || defined(__i386__)
#include <sys/io.h>
#endif
#include <sys/mount.h>
#include <sys/reboot.h>
#include <termios.h>
#include <unistd.h>

#include "lib/util.h"
#include "lib/virtio_pci.h"
#include "lib/virtio_mmio.h"
#include "lib/vring.h"
#include "lib/vring_packed.h"
#include "tests/test.h"

#define QUEUE_SIZE 16

static int term_width(void)
{
    struct winsize ws;
    if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        return ws.ws_col;
    return 80;
}

/* Pretty listing for interactive use; columns adapt to terminal width. */
static void list_tests(void)
{
    int n = test_count();
    int name_w = 4, sec_w = 7;
    for (int i = 0; i < n; i++) {
        struct test_entry *t = test_get(i);
        int nl = (int)strlen(t->name);
        int sl = (int)strlen(t->spec_section);
        if (nl > name_w) name_w = nl;
        if (sl > sec_w) sec_w = sl;
    }
    int ver_w = 4;
    int width = term_width();
    int fixed = name_w + 1 + ver_w + 1 + sec_w + 1;
    int desc_w = width - fixed;
    if (desc_w < 20) desc_w = 20;

    for (int i = 0; i < n; i++) {
        struct test_entry *t = test_get(i);
        int dl = (int)strlen(t->desc);
        if (dl <= desc_w) {
            printf("%-*s %-*s %u.%u %s\n",
                   name_w, t->name,
                   desc_w, t->desc,
                   t->spec_version >> 8, t->spec_version & 0xff,
                   t->spec_section);
        } else {
            printf("%-*s %.*s~ %u.%u %s\n",
                   name_w, t->name,
                   desc_w - 1, t->desc,
                   t->spec_version >> 8, t->spec_version & 0xff,
                   t->spec_section);
        }
    }
}

/* TAB separated listing for the runner: name, desc, version, section. */
static void list_tests_tsv(void)
{
    int n = test_count();
    for (int i = 0; i < n; i++) {
        struct test_entry *t = test_get(i);
        printf("%s\t%s\t%u.%u\t%s\n", t->name, t->desc,
               t->spec_version >> 8, t->spec_version & 0xff,
               t->spec_section);
    }
}

static const char *result_str(test_result_t r)
{
    switch (r) {
    case TEST_PASS:   return "PASS";
    case TEST_FAIL:   return "FAIL";
    case TEST_SKIP:   return "SKIP";
    case TEST_WEDGED: return "WEDGED";
    case TEST_REJECT: return "REJECT";
    case TEST_XFAIL:  return "XFAIL";
    case TEST_XPASS:  return "XPASS";
    }
    return "????";
}

/*
 * Apply TEST_FLAG_XFAIL remapping. A test registered with XFAIL
 * documents a known VMM bug: a FAIL outcome is expected and becomes
 * XFAIL, while an unexpected PASS becomes XPASS so the stale marker
 * is noticed and removed. REJECT, WEDGED and SKIP are left alone:
 * they signal that the test did not actually exercise the bug, and
 * silently folding them into XFAIL would hide unrelated regressions.
 * Tests that flag a "device stayed silent" path as the documented
 * bug must therefore translate that path to TEST_FAIL themselves.
 */
static test_result_t apply_xfail(struct test_entry *t, test_result_t r)
{
    if (!(t->flags & TEST_FLAG_XFAIL))
        return r;
    if (r == TEST_PASS)
        return TEST_XPASS;
    if (r == TEST_FAIL)
        return TEST_XFAIL;
    return r;
}

/*
 * Map a final verdict to the init exit code. Only TEST_FAIL and the
 * synthetic TEST_XPASS surface as failures; XFAIL is the expected
 * outcome for a flagged test.
 */
static int verdict_failed(test_result_t r)
{
    return (r == TEST_FAIL || r == TEST_XPASS) ? 1 : 0;
}

static int run_test(struct test_entry *t)
{
    struct virtio_dev dev;

    if (virtio_pci_find(t->device_id, &dev) < 0) {
        printf("[SKIP] %s (no device 0x%04x)\n", t->name, t->device_id);
        return 0;
    }

    if (virtio_pci_init(&dev) < 0) {
        printf("[FAIL] %s (device init failed)\n", t->name);
        return 1;
    }

    uint16_t nq = dev.common->num_queues;
    if (nq == 0)
        nq = 1;

    /* Set up all queues the device requires */
    struct vring queues[16];
    for (uint16_t q = 0; q < nq && q < 16; q++) {
        vring_alloc(&queues[q], QUEUE_SIZE);
        vring_attach(&dev, &queues[q], q);
    }

    /*
     * Queue selection: if the test specifies a queue, use it.
     * 0xFF stored (VV_QUEUE_LAST+1) means last queue (e.g. net controlq).
     * Otherwise, default to TX (queue 1) for net and vsock.
     */
    uint16_t test_q = 0;
    if (t->queue_idx == 0xFF) {
        test_q = nq - 1;
    } else if (t->queue_idx > 0) {
        test_q = t->queue_idx - 1;
    } else if (nq > 1 && (t->device_id == VIRTIO_PCI_DEVICE_NET ||
                          t->device_id == VIRTIO_PCI_DEVICE_VSOCK)) {
        test_q = 1;
    }

    /* DRIVER_OK */
    dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    test_fn fn = (test_fn)t->fn;
    test_result_t result = apply_xfail(t, fn(&dev, &queues[test_q]));
    printf("[%s] %s\n", result_str(result), t->name);

    virtio_pci_reset(&dev);
    return verdict_failed(result);
}

static int run_test_packed(struct test_entry *t)
{
    struct virtio_dev dev;
    struct vring_packed vr;

    if (virtio_pci_find(t->device_id, &dev) < 0) {
        printf("[SKIP] %s (no device 0x%04x)\n", t->name, t->device_id);
        return 0;
    }

    if (virtio_pci_init_packed(&dev) < 0) {
        printf("[SKIP] %s (packed queues not supported)\n", t->name);
        return 0;
    }

    uint16_t nq = dev.common->num_queues;
    if (nq == 0)
        nq = 1;

    struct vring_packed extra[16];
    vring_packed_alloc(&vr, QUEUE_SIZE);
    vring_packed_attach(&dev, &vr, 0);
    for (uint16_t q = 1; q < nq && q < 16; q++) {
        vring_packed_alloc(&extra[q], QUEUE_SIZE);
        vring_packed_attach(&dev, &extra[q], q);
    }

    struct vring_packed *test_vr = &vr;
    if (nq > 1 && (t->device_id == VIRTIO_PCI_DEVICE_NET ||
                   t->device_id == VIRTIO_PCI_DEVICE_VSOCK))
        test_vr = &extra[1];

    /* DRIVER_OK */
    dev.common->device_status |= VIRTIO_STATUS_DRIVER_OK;
    __sync_synchronize();

    test_packed_fn fn = (test_packed_fn)t->fn;
    test_result_t result = apply_xfail(t, fn(&dev, test_vr));
    printf("[%s] %s\n", result_str(result), t->name);

    virtio_pci_reset(&dev);
    return verdict_failed(result);
}

static int run_test_mmio(struct test_entry *t)
{
    struct virtio_mmio_dev dev;

    if (virtio_mmio_find(&dev) < 0) {
        printf("[SKIP] %s (no MMIO device found)\n", t->name);
        return 0;
    }

    /* MMIO tests manage their own init - pass raw device */
    test_mmio_fn fn = (test_mmio_fn)t->fn;
    test_result_t result = apply_xfail(t, fn(&dev));
    printf("[%s] %s\n", result_str(result), t->name);

    virtio_mmio_reset(&dev);
    return verdict_failed(result);
}

/*
 * Parse vv.test= from /proc/cmdline.
 * Returns the test name, "all", "list", or NULL if not present.
 */
static const char *parse_cmdline_test(void)
{
    static char buf[4096];
    FILE *f = fopen("/proc/cmdline", "r");
    if (!f)
        return NULL;
    if (!fgets(buf, sizeof(buf), f)) {
        fclose(f);
        return NULL;
    }
    fclose(f);

    char *p = strstr(buf, "vv.test=");
    if (!p)
        return NULL;
    p += strlen("vv.test=");

    /* Terminate at space or newline */
    char *end = p;
    while (*end && *end != ' ' && *end != '\n')
        end++;
    *end = '\0';
    return p;
}

static void shutdown(int failures)
{
    fflush(stdout);
    fflush(stderr);

    /*
     * The verdict marker we just printed sits in the kernel TTY
     * transmit buffer. Drain the local TTY layer first.
     */
    tcdrain(STDOUT_FILENO);
    tcdrain(STDERR_FILENO);

    /*
     * tcdrain only flushes the guest TTY into the hvc/virtio-console
     * virtqueue. The VMM still has to consume that virtqueue and push
     * the bytes into the host pipe. That step is asynchronous. If we
     * trigger the VMM exit immediately (port 0x501 on CH, ACPI off on
     * QEMU) the unread tail of the virtqueue is dropped on the floor
     * and the host scrapes a marker less console, classifying the
     * test as FAIL or WEDGED at random. Under heavy host parallelism
     * the race opens wide enough to lose the marker on every run.
     *
     * The host runner kills the VM as soon as it sees a result marker
     * on the console, so under the normal runner we do not have to
     * self exit at all. Wait here for that kill. As a fallback for
     * standalone use (no host scraper), bail out via the slow path
     * after a generous timeout so the byte stream has time to drain.
     */
    for (int i = 0; i < 200; i++)
        usleep(50 * 1000);

    int fd = open("/dev/port", O_WRONLY);
    if (fd >= 0) {
        uint8_t val = 0x01;
        lseek(fd, 0x501, SEEK_SET);
        ssize_t r = write(fd, &val, 1);
        (void)r;
        close(fd);
    } else {
#if defined(__x86_64__) || defined(__i386__)
        if (iopl(3) != 0)
            ioperm(0x501, 1, 1);
        outb(0x01, 0x501);
#endif
    }

    reboot(RB_POWER_OFF);
    _exit(failures ? 1 : 0);
}

static void print_banner(FILE *f)
{
    fprintf(f, "        _      _   _\n"
              " /\\   /(_)_ __| |_(_) ___\n"
              " \\ \\ / / | '__| __| |/ _ \\\n"
              "  \\ V /| | |  | |_| | (_) |\n"
              "   \\_/ |_|_|   \\__|_|\\___/\n"
              "   /\\   /(_) | | __ _(_)_ __\n"
              "   \\ \\ / / | | |/ _` | | '_ \\\n"
              "    \\ V /| | | | (_| | | | | |\n"
              "     \\_/ |_|_|_|\\__,_|_|_| |_|\n\n");
}

int main(int argc, char **argv)
{
    /* Optional banner on the host (--banner). Hidden by default to
     * keep machine-readable output (e.g. from --list) clean. */
    if (argc > 1 && strcmp(argv[1], "--banner") == 0) {
        print_banner(stdout);
        return 0;
    }

    /* Handle --list when run on the host */
    if (argc > 1 && strcmp(argv[1], "--list") == 0) {
        list_tests();
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--list-tsv") == 0) {
        list_tests_tsv();
        return 0;
    }

    /* Refuse to run tests unless we are PID 1 (inside a VM) */
    if (getpid() != 1) {
        fprintf(stderr,
                "Usage: ./init --list | --list-tsv | --banner\n");
        fprintf(stderr, "Run './run -m <vmm>' to execute tests in a VM.\n");
        return 1;
    }

    /* Mount procfs and sysfs (we are PID 1 in initramfs) */
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    printf("\n[vv] virtio-villain\n\n");

    const char *test_name = parse_cmdline_test();

    if (!test_name || strcmp(test_name, "all") == 0) {
        /* Run all tests */
        int failures = 0;
        int n = test_count();
        for (int i = 0; i < n; i++) {
            struct test_entry *te = test_get(i);
            if (te->flags & TEST_FLAG_MMIO)
                failures += run_test_mmio(te);
            else if (te->flags & TEST_FLAG_PACKED)
                failures += run_test_packed(te);
            else
                failures += run_test(te);
        }
        printf("\n%d/%d tests passed\n", n - failures, n);
        shutdown(failures);
    }

    if (strcmp(test_name, "list") == 0) {
        list_tests();
        shutdown(0);
    }

    /* Run a single named test */
    struct test_entry *t = test_find(test_name);
    if (!t) {
        printf("Unknown test: %s\n", test_name);
        _exit(1);
    }
    int fail;
    if (t->flags & TEST_FLAG_MMIO)
        fail = run_test_mmio(t);
    else if (t->flags & TEST_FLAG_PACKED)
        fail = run_test_packed(t);
    else
        fail = run_test(t);
    shutdown(fail);
}

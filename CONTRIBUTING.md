# Contributing

Contributions are welcome. The project is a small static C harness plus
per-device test files; most contributions fall into a few categories:

## Adding a Test

1. Pick the right category under `tests/<device>/` and the next free
   numeric ID for that category.
2. Use the existing tests as templates. Each test is a single C file
   that calls `REGISTER_TEST(...)` at the bottom.
3. Reference the spec section your test exercises in the comment header
   and in the `REGISTER_TEST` arguments.
4. Build with `make` and run via `./run -m <vmm> <TEST_ID>`.

### Tests that need a host side action

Some tests require something to happen on the host while the guest is
running, hot plug, hot unplug, save and restore, snapshot, pause, on
the fly config change. For those, drop a Python file next to the C
file with the same numeric ID, for example `tests/blk/b0200_*.c` and
`tests/blk/b0200_*.py`. The runner discovers it automatically and
spawns its `run(ctx)` on a background thread.

The `ctx` object exposes `backend.name` (`ch` or `qemu`), `vmm_pid`,
`tmpdir` (private, removed at the end), `api_sock`, `disk`, `log()`,
`wait_text(pattern, timeout)` against the shared guest output, a
`stop_event`, and a `vm_api(command, args)` helper. `vm_api` shells
out to `ch-remote` on Cloud Hypervisor and speaks QMP on QEMU. Skip
cleanly when `ctx.backend.name` does not match or `ctx.api_sock` is
`None`. The C side should report SKIP if the host action did not
happen, FAIL only if the host did its part and the device failed the
spec assertion. See [README.md](README.md) section "Host side
sidecars" for the conceptual difference between these tests and the
VMM integration test suites.

## Adding Support for a New Device

1. Add device id to `lib/virtio_pci.h`.
2. Create `lib/virtio_<device>.c/.h` if device specific configuration
   handling is required, otherwise reuse the generic transport helpers.
3. Add a new `tests/<device>/` directory and pick a unique single
   letter prefix for test IDs.
4. Update `bin/init.c` device discovery if a new probing helper is
   needed.
5. Update `README.md` Coverage table.

## Coding Style

- C89 with selected C99 features, `-Wall -Wextra -Werror`.
- No dynamic allocation in the harness path; tests use a fixed page
  pool through `vv_alloc_pages`.
- Static functions per file, no cross-file symbol leakage outside the
  `lib/` API.
- Keep tests deterministic. No timing dependent assertions.

## Commit Messages

- One logical change per commit.
- Subject line: `tests: <ID> <short description>` for tests,
  `<area>: <subject>` for everything else. Capital letter after the
  prefix. Hard wrap body at 72 columns.

## Sign-off

All commits must be signed off (`git commit -s`) under the Developer
Certificate of Origin.

## Discussion

Open an issue first for non trivial changes (new device support,
framework changes, public API additions in `lib/`).

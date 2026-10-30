# External appliance suites

The default suite in Virtio Villain is still the virtio test appliance. Its device
setup, source lists, command line and automatic guest kernel download are
unchanged. Other suites can use the framework without inheriting virtio
discovery, PCI/MMIO setup, negotiation, or queue fixtures.

An external suite contains its own tests, prerequisites, kernel contract and
launch policy. It need not be stored in this repository. The public extension
points contain no device or kernel interfaces specific to an external suite.

## Framework components

| Component | Responsibility |
| --- | --- |
| `lib/test.h` | Result types and diagnostic return helpers with typed test registration |
| `lib/test_runner.h` / `.c` | Test selection and contexts with cleanup and result reporting |
| `lib/appliance_init.c` | PID 1 startup and mounts with kernel command line selection and shutdown |
| `lib/python/villain/initramfs.py` | Reproducible packaging of a static ELF executable as `/init` |
| `lib/python/villain/appliance.py` | Catalog discovery, launch supervision, console capture, result validation and JSON reporting |
| `lib/python/villain/reporting.py` | Shared human result rows, terminal progress, summaries and failure lists |
| `run --suite PATH` | Load a trusted suite directory or explicit policy file |
| `make appliance-initramfs` | Build an explicit external source list independently of virtio |

The shared Python package is `villain`, located under `lib/python`.
The runner locates it from its own checkout, including when invoked from
another working directory. No package installation or `PYTHONPATH` setting
is required. C sources and headers remain directly under `lib`.

Shared host helpers are available in these modules:

| Module | Interface |
| --- | --- |
| `villain.cli` | Banner, help formatter and memory argument normalization |
| `villain.console` | Separate diagnostic capture, terminal handling and stream merging |
| `villain.process` | Supervised process launch, separate stream capture and termination |
| `villain.vmm` | Portable launch settings, VMM backends, virtio fixtures and guest kernel selection |
| `villain.diagnostics` | VMM output classification shared by tests and fuzzing |
| `villain.host` | Capability inspection and explicitly requested host tuning |
| `villain.artifacts` | GitHub release assets with published SHA256 verification and cache reuse |
| `villain.build` | Suite build metadata and shared build dispatch |
| `villain.images` | Raw, qcow2, fixed VHD and VHDX image creation |

Importing these modules does not launch a VM or apply host tuning.
The artifact helper checks the latest release by default.
`prefer_cached=True` reuses recorded release metadata and verifies the
cached asset before use. `offline=True` forbids network access and cache
writes. External suites still select their own kernel build or image
policy.

`VmmBackend.build_launch_cmd(LaunchConfig(...))` builds launch arguments
without test device fixtures. `LaunchConfig` takes a positive CPU count and
a memory size such as `512M`. Optional boot fields are `kernel`, `initramfs`,
`cmdline` and `firmware`. Each backend translates these settings to its own
syntax. Backend options remain explicit keyword arguments. Unsupported boot
modes raise an error. Add device arguments separately. Building a command
does not start a process or apply host tuning.

`villain.images.create(path, size, image_format="raw")` creates a disk image.
Sizes are specified in bytes. Container capacity alignment follows
`qemu-img`. `create_raw(path, size, exclusive=True)` refuses existing paths.
These helpers do not choose a partition layout or bootloader.

`villain.process.capture(argv, timeout=seconds)` returns a `CompletedProcess`
with separate byte streams. A timeout raises `TimeoutExpired` with captured
output after terminating the process group. `terminal=True` supplies a private
controlling terminal without changing the caller's terminal.

The existing `build_cmd(...)` methods describe virtio test environments.
Neither interface establishes the privileges required by another suite.
Completion protocols and deadlines remain the responsibility of the caller.
External recovery also remains the caller's responsibility.

## Guest interface

Include `lib/test_runner.h` and register tests in a dedicated linker section:

```c
struct context {
    int resource;
};

static test_result_t example(struct context *context)
{
    if (context->resource < 0)
        TFAIL("resource: expected a usable descriptor, actual %d", context->resource);
    return TEST_PASS;
}

VV_REGISTER_TEST(example_registry, EX001, example, "example resource operation");
```

The registration macro creates an adapter for the typed context. It does not
cast incompatible function pointers. Define a `struct vv_test_suite` with:

- A contract identifier and protocol prefix without whitespace, such as
  `example-v1` and `VV`.
- Registry bounds (`__start_example_registry`, `__stop_example_registry`).
- `context_size` and an optional `init_context` callback.
- An optional `preflight` callback. Zero means prerequisites are met.
  Nonzero means an infrastructure problem, not a skipped or successful test.
- A `cleanup` callback returning a test verdict. It runs after every returned
  test outcome, including assertions and skips. Cleanup failure fails the test.

Call `vv_appliance_main(argc, argv, &suite)` from the `main` function of the suite.
`--list` is safe on the host and prints `ID<TAB>description`. Test execution
requires PID 1. The appliance mounts procfs, sysfs and devtmpfs and reads
`vv.run=RUN_ID` and optional `vv.tests=ID,ID` from `/proc/cmdline`.

For a shared immutable boot image, pass `vv.config=smbios` instead of
`vv.run` and `vv.tests`. Set the SMBIOS Type 1 serial number to the first
32 characters of the run ID. Set the Type 1 version to `vv:` followed by
the remaining characters. Set the Type 1 SKU to the selected test IDs
separated by `+`. Run IDs may contain letters, digits, `_` and `-` and
must not exceed 61 characters. The encoded selection must not exceed
32 characters. This mode requires `CONFIG_DMI=y` and `CONFIG_DMIID=y`.
Missing, invalid or mixed configuration is an infrastructure failure.

Tests return `TEST_PASS`, `TEST_FAIL`, or `TEST_SKIP`. Other verdicts fail unless
a suite applies its own remapping before returning. Use skips only for optional
capabilities. Missing execution prerequisites belong in `preflight`. An empty,
unexecuted, or entirely skipped suite is not a pass.

`TFAIL` and related helpers return immediately. Keep owned resources in the
context so the cleanup callback can release them. These callbacks cannot
recover from a kernel hang or a test that does not return.

## Build a separate appliance

Suites with build metadata can be built from either checkout. From the
framework directory:

```sh
make SUITE=/path/to/suite init
make SUITE=/path/to/suite initramfs
make SUITE=/path/to/suite kernel
./run --suite /path/to/suite --build EX001
```

The suite can expose the same targets through its own Makefile using the
shared Make include. Both entry points use the suite's source selection,
preparation actions and output layout. Plain framework builds are unchanged.

`--build` explicitly builds the executable and initramfs before execution.
It does not build a kernel. Running without it never builds implicitly.
Suites without build metadata continue to support supplied binaries and
archives. Requesting a build from such a suite is an error.

Use `TARGET=custom` with Make and `--build-dir custom` with the runner to
select the same output root. Relative paths are resolved from the suite
directory, not the calling directory. The runner selects the corresponding
executable and archive. Successful kernel actions record the actual produced
image, architecture and format in `kernel-result.json` under that output root.
The runner uses this result even when the kernel was built elsewhere.
Missing results require an explicit kernel build or a supplied image.
Explicit `--kernel` overrides
remain supported. `--build` cannot be combined with explicit `--binary` or
`--initramfs` overrides. `--output` selects diagnostics and runtime
artifact workspaces for each execution. Reusable build outputs remain
separate.

Source and appliance output paths must not contain whitespace because the
underlying Make appliance rules use whitespace separated source lists.
Optional preparation and kernel actions belong to the suite. They are not
inferred from its name or added to the default virtio build.

The lower level explicit source interface remains available:

Provide an explicit source list, include paths and output directory for the suite:

```sh
make appliance-initramfs CC=gcc \
  APPLIANCE_SRCS="/work/example/init.c /work/example/cases.c" \
  APPLIANCE_CPPFLAGS="-I/work/example" \
  APPLIANCE_TARGET=/work/example/build
```

The framework links the generic guest runner and PID 1 startup automatically.
Outputs are `build/init` and `build/initramfs.cpio.gz`. Generated headers must be
prepared by the external suite before invoking this target. Choose a separate
output directory for each suite and compiler configuration.

All initramfs targets share the deterministic Python newc/gzip packager.
The built-in virtio, fuzz and performance targets strip temporary copies
using `STRIP`; external appliances retain their input binary unchanged by
default. Original executables are never modified. Packaging does not require
the `cpio` or `gzip` commands.

External sources are never added to the virtio `SRCS` wildcards. The existing
`init`, `initramfs`, fuzz and performance targets continue to build their
original executables and archives.

Kernel construction remains independent. Use `build-kernel --source ...`
with the config supplied by the kernel and an explicit external `--output` and
`--cache-dir`. A suite can declare its default kernel image. Use `--kernel PATH`
to override it, or to supply an image when the suite has no default.
The runner never downloads or builds a kernel implicitly.

## Host policy interface

Create `lib/suite.py` inside an external suite directory:

```python
from pathlib import Path
from villain.appliance import Suite

ROOT = Path(__file__).resolve().parents[1]

class ExampleSuite(Suite):
    def add_arguments(self, parser):
        parser.add_argument("--vmm", default="cloud-hypervisor")

    def build_command(self, args, selected, run_id):
        cmdline = (
            "console=ttyS0 rdinit=/init "
            f"vv.run={run_id} vv.tests={','.join(selected)}"
        )
        return [
            args.vmm, "--kernel", str(args.kernel),
            "--initramfs", str(args.initramfs), "--cmdline", cmdline,
            "--serial", "tty", "--console", "off",
        ]

def create_suite():
    return ExampleSuite(
        name="example", contract="example-v1",
        binary=ROOT / "build/init",
        initramfs=ROOT / "build/initramfs.cpio.gz",
        kernel=ROOT / "build/kernel/images/bzImage-x86_64",
        requires_control=True,
    )
```

This policy runs within the framework process, not through another full test
runner. The framework owns listing, selection, supervision, protocol checks
and reporting. The policy supplies launch arguments and prerequisite checks.
Only explicitly requested modules are loaded. Do not load untrusted code.

The optional `kernel` field supplies the default image. Use a path rooted at
the suite directory so execution does not depend on the working directory.
The runner rejects missing or empty images before launch. Set
`kernel_build_hint` to show the suite's build command when its default image
is unavailable. An explicit `--kernel` override is validated independently.
Listing tests does not require a built kernel.

For preparation that must run within the appliance timeout, return
`self.execution_command(argv)` from `build_command`. Override `execute(argv)`
to prepare the boot environment and replace its process with the VMM using
`os.execvp`. Preparation and VM execution then share console capture, the
deadline and recovery controls. Returning from `execute` is an infrastructure
failure. Suites that need no preparation can continue returning a VMM command
directly.

Each call to `build_command` receives `args.workspace` as an absolute
`Path`. This directory already exists and is unique to the test and
repetition. It is under `artifacts/ITERATION-TEST_ID/` in the diagnostics
directory. The framework passes a separate argument namespace to each
call. The parsed invocation arguments are not changed. Suites that need
no workspace can ignore it.

Write runtime artifacts below this directory. For example, pass
`str(args.workspace / "boot")` as an output argument to the preparation worker
when its image builder requires a new directory. Pass the path explicitly
through `self.execution_command(...)`. The worker does not inherit the
parent argument namespace. Preparation should still happen inside
`execute`, not while constructing the command.

By default, diagnostics and workspaces are below
`BINARY_DIRECTORY/results/TIMESTAMP-ID/`. Selecting `--build-dir` changes that
default along with the binary location. An explicit `--output PATH` places
diagnostics and workspaces under that new directory instead. Each JSON
run records its absolute `workspace` alongside its `console` path and run
ID. These paths remain in the report after preparation errors and
timeouts. They are also retained after interruption. If workspace creation
itself fails, the report records the requested path and the error.
Runtime artifacts are retained with the report on success and failure.
Suites should remove temporary assembly files without deleting the
retained workspace. Reusable build artifacts and download caches remain
separate. The framework does not copy or rebuild them for each test.

```sh
./run --suite /work/example --list
./run --suite /work/example \
  --vmm /work/bin/cloud-hypervisor \
  --control /work/bin/external-control --repeat 3 EX001
```

Put `--suite PATH` first. A directory selects its `lib/suite.py`.
Explicit policy file paths are also supported.
Private Python helpers belong beside the policy and use relative imports,
such as `from .contract import CONTRACT`. Each loaded policy has a unique
package namespace, separate from the framework's `villain` package and from
other suites. Relative imports also work inside hooks called after loading.
The loader does not add private directories to `sys.path`.

`--help` after `--suite PATH` shows both common and suite
options. `--list` does not run the host preflight, boot an appliance, or require
a kernel or recovery control.

Test ID matching ignores letter case. The runner uses registered IDs in
guest commands and results. Selecting the same ID more than once is an
error, even when its spelling differs only in letter case. Catalog IDs
must also be unique without regard to letter case.

A policy can override `list_tests(self, binary)` to return IDs and
descriptions from its sources before an executable has been built.
`villain.appliance.source_catalog(paths, macro)` reads literal registration
descriptions with either three arguments or an initial registry argument.
It ignores comments and rejects empty, duplicate or malformed entries.
This is a source index, not a C preprocessor. Execution always uses the
compiled executable's catalog, including its conditional build choices.

Each selected test runs in a fresh appliance VM with its own run identifier,
console log and timeout. `--repeat N` repeats that sequence N times.
Use `--jobs N` or `-j N` to run up to N appliance VMs concurrently.
The default is one. Tests and repetitions share the concurrency limit.
Use `--console` or `-c` to stream VM output live while retaining each complete
console log. Console mode runs serially even when `--jobs` requests more VMs.
It suppresses progress updates and individual host result rows, as in the
virtio runner. The final summary, failure list and diagnostics location remain
available. Without `--console`, execution and reporting are unchanged.
Completed results are saved as they arrive and listed in selection order
in the JSON report. Each test retains its own VM and diagnostics.
Assertion failures, guest crashes and timeouts do not prevent later tests
from running once the previous launcher has been terminated.

Execution stops on interruption or failed launcher termination. It also
stops if external control cannot arm, recover or disarm. Results completed
before the stop remain in the report. Any failure keeps the overall result
failing even when later tests pass.

During parallel execution, interruption or a safety control failure stops
new launches and cancels active VMs through the normal cleanup path.
External controls used with parallel jobs must support independent run IDs
and simultaneous control requests.

## Result and process contract

With protocol prefix `VV`, the guest emits:

```text
VV-BEGIN RUN_ID CONTRACT
VV-RUN RUN_ID EX001
VV-RESULT RUN_ID EX001 PASS
VV-SUMMARY RUN_ID 1 0 0 0
VV-END RUN_ID 0
```

Summary counters are pass, fail, skip, infrastructure. The host checks the run
identifier and contract. It verifies execution ordering and exact selected
IDs. It also checks individual verdicts, counters and final status.
Missing, stale, duplicate, malformed or
incomplete records are infrastructure failures. Ordinary diagnostic lines are
preserved in the console log.

The launcher must remain attached until the appliance shuts down, relay its
console to stdout/stderr, clean up its own subordinate processes, and return
zero for a completed boot/shutdown. Test failures are conveyed by guest
records, not by the exit code of the launcher. A nonzero launcher exit is an
infrastructure failure.

Suites whose guests cannot power off can opt into `host_shutdown=True` and
pass `vv.shutdown=host` on the kernel command line. After result reporting,
cleanup and synchronization, the guest emits `vv-appliance-stop RUN_ID` and
waits without requesting poweroff. The runner validates the complete result
protocol and matching stop marker before terminating the attached launcher.
The launcher must support SIGTERM without leaving subordinate processes.
The report records `host_shutdown` when the runner stops the launcher.
Missing completion markers, malformed results and kernel panics remain
infrastructure failures. This mode does not validate guest poweroff.
The default shutdown mode and virtio appliance operation are unchanged.

Exit codes are 0 (pass), 1 (assertion/cleanup failure), and 2 (infrastructure,
including timeouts and entirely unexecuted suites). The diagnostics directory
contains `results.json` and artifact workspaces for each execution. It also
retains console and external control logs. The JSON report records image
hashes and exact launch arguments. Each run entry includes a workspace
path and test verdicts.

Human output uses the same aligned result rows, terminal colors, progress,
count summary and failure lists as the virtio runner. Test descriptions come
from the suite catalog. External suites leave virtio specification columns
empty. Infrastructure problems remain `INFRA`, including failures after an
otherwise passing guest result. Failure output includes console paths and
diagnostic excerpts. Repeated cases identify their run in the description.

## External timeout and recovery

Setting `requires_control=True` makes `--control` mandatory. The executable is
called without a shell:

```text
CONTROL arm --run-id ID --timeout SECONDS
CONTROL recover --run-id ID
CONTROL disarm --run-id ID
```

`arm` must establish a watchdog **outside the machine under test** and return
zero only after it is active. `recover` must reset or power cycle the test
environment and confirm completion. `disarm` cancels the external
watchdog. A failed recovery leaves it armed. All operations must be idempotent
for the run identifier and return nonzero with diagnostics on failure.

The framework also enforces a process deadline and attempts bounded process
termination. That does not contain a kernel failure or recover a hung host.
The control executable must contact a real management service. A local
sleep or a script that does nothing is not external recovery. Killing the
test process is not external recovery either.
The external suite and execution environment define hosting and recovery policy.

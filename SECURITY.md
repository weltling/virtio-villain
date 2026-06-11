# Security Policy

## Scope

virtio-villain is a test framework. Its purpose is to find bugs in
VMMs, not to be a deployable component itself. The harness runs as
`init` inside an ephemeral guest with no network or persistent state,
so the threat surface of the framework itself is small.

## Reporting Bugs in Target VMMs

If a test in this repository triggers a crash, panic, or memory
safety issue in a third party VMM (Cloud Hypervisor, crosvm, QEMU,
Firecracker, ...), report it directly to that project's security
contact, not here. Do not file public issues describing reproducers
for unpatched VMM bugs.

Most upstreams use:

- Cloud Hypervisor: see SECURITY.md in the cloud-hypervisor repo.
- crosvm: see crosvm security policy.
- QEMU: qemu-security@nongnu.org.
- Firecracker: see firecracker security policy.

When you do file an upstream report, you may reference virtio-villain
test IDs (for example `T0042`, `N0017`) in the bug description.

## Reporting Issues in virtio-villain Itself

For issues in the harness, build system, or test logic that does not
involve a VMM bug, open a regular issue or pull request.

For sensitive issues in the harness itself, use GitHub Private
Vulnerability Reporting (Security tab in this repository) or email
the maintainer (personal capacity) at anbelski@linux.microsoft.com.

If you are unsure whether a finding is security sensitive, treat it
as such and use the project's private reporting channel first.

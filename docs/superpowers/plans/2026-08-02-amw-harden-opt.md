# AMW Harden + Opt Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans or implement task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Harden AMW readiness/Enable/MessageInfo semantics and improve Cyclone wait + discovery callbacks + WaitForService.

**Architecture:** Two phases — A correctness (adapter, FastDDS gate, RegisterBuiltin, tests), then B WaitSet / unlock-on-callback / WaitForService notify. No DdsBackend abstraction.

**Tech Stack:** C++17, FastDDS 3.x / Cyclone ddsc, GTest

**Spec:** `docs/superpowers/specs/2026-08-02-amw-harden-opt-design.md`

**Commits:** Only when user explicitly asks.

---

## File map

| Path | Change |
|---|---|
| `amw/network_adapter.hpp` | Fix false enabled |
| `amw/dds/dds_stub_provider.cpp` | stub Start→false; RegisterBuiltin not overwrite Cyclone |
| `amw/dds/fastdds_provider.cpp` | listener enabled gate; MessageInfo; unlock OnTopologyRaw |
| `amw/dds/cyclonedds_provider.cpp` | WaitSet; MessageInfo; unlock OnTopologyRaw |
| `amw/amw_test.cpp` | PreferReady / framed / plugin / enable tests |
| `service/client_base.hpp` | WaitForService notify |
| `service_discovery/*` | light change notify if needed |
| docs/scripts | path / preflight / comment |

---

### Task 1: False readiness + stub Start

- [x] Fix `NetworkTransmitter` / `NetworkReceiver` enable rules
- [x] `DdsStubDiscoveryProvider::Start` return false
- [x] Update stub tests if needed

### Task 2: FastDDS enable gate + MessageInfo

- [x] Listener checks `enabled_`
- [x] Align MessageInfo fields with Cyclone
- [x] Optional writer/reader enable API if available (atomic gate + drain)

### Task 3: RegisterBuiltin cleanup

- [x] Do not force-register Cyclone as stub; use Create* factories

### Task 4: Discovery unlock + Cyclone WaitSet

- [x] Copy callbacks then unlock in both OnTopologyRaw
- [x] Replace 5ms poll with waitset + timeout

### Task 5: WaitForService event wake

- [x] condition_variable + topology change listener (or manager notify)

### Task 6: Tests + docs/scripts

- [x] PreferReady, framed MessageInfo, plugin load, Enable gate
- [x] Docs/preflight/topology comment
- [x] Docker: amw_test, node_test, both sim scripts

---

## Verification

```bash
# In SpaceHero Docker
cmake --build build -j --target autolink.amw.amw_test autolink.node.node_test amw_opendds
./build/bin/autolink.amw.amw_test
./build/bin/autolink.node.node_test
./scripts/amw_sim_dual_host.sh amw_cyclonedds 0 10
./scripts/amw_sim_service.sh amw_cyclonedds 0 15
```

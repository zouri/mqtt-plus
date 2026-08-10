---
status: accepted
implementation: planned
---

# Run Python and native processors out of process

## Context

Python brings interpreter and dependency-environment concerns, while user-built native processors can crash the application or depend on incompatible compiler and Qt ABIs. Loading either runtime directly into the GUI process would expand the failure and compatibility boundary.

## Decision

Future Python and native C++ processor runtimes will execute in versioned helper processes behind the same `MessageProcessorEngine` interface used by the in-process Lua and JavaScript adapters. Communication will use framed CBOR messages, with a stable C ABI at the native helper boundary. The application will not load arbitrary user-built dynamic libraries into the GUI process.

## Consequences

Python and C++ preparation will require content-addressed runtime artifacts, helper lifecycle management, crash recovery, and explicit local readiness. Their storage, subscription bindings, history identity, and normalized result contract remain the same as Lua and JavaScript.

This decision is accepted but not implemented. The current application registers only the in-process Lua and JavaScript runtime adapters.

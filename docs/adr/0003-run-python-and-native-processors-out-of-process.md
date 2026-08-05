---
status: accepted
---

# Run Python and native processors out of process

Future Python and native C++ Message Processor runtimes will execute in versioned helper processes behind the same `MessageProcessorEngine` seam used by in-process Lua and JavaScript adapters. Framed CBOR messages and a stable C ABI isolate the GUI process from the CPython GIL, dependency environments, native crashes, and compiler or Qt ABI changes; arbitrary user-built dynamic libraries will not be loaded into the main application process.

## Consequences

Python and C++ preparation requires content-addressed runtime artifacts, helper lifecycle management, and explicit local readiness. Their storage, subscription bindings, history identity, and normalized result contract remain identical to Lua and JavaScript.

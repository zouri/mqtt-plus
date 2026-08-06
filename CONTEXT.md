# MQTT Workbench

This context describes the language used to prepare and publish MQTT messages and to transform received MQTT payloads in the workbench.

## Publishing

**Publish Composer**:
The transient workspace for preparing one MQTT publish. Its contents do not become a Publish Draft until the user explicitly saves them.
_Avoid_: Publish draft, offline queue

**Publish Draft**:
A persistent message definition that the user explicitly saves under a name unique within the Draft Library for repeated publishing. It contains reusable payload, payload format, QoS, and retain settings, may contain a default topic that can be replaced before publishing, and may include a searchable description that is never published. Publishing, switching connections, and restarting the application do not remove it; only explicit deletion does. It is distinct from unsaved composer contents, publish history, and messages waiting for delivery.
_Avoid_: Publish history, pending message, offline queue

**Draft Library**:
The application-wide collection of publish drafts available across MQTT connections.
_Avoid_: Session drafts, connection drafts

**Quick Publish**:
An explicit action that immediately publishes a saved Publish Draft through the current MQTT connection without changing the current composer contents. It uses the draft's default topic when present; otherwise, the user supplies a topic for that publish only.
_Avoid_: Load draft, queued publish

## Message Processing

**Message Processor**:
An application-wide reusable definition that transforms one received MQTT message into a structured result. A Message Processor is identified independently of the language used to implement it.
_Avoid_: Script, decoder, parser script

**Processor Library**:
The application-wide collection of Message Processors available for subscription bindings.
_Avoid_: Script library, session processors

**Processor Revision**:
An immutable saved version of a Message Processor. Editing and saving a processor creates another revision instead of changing an existing revision.
_Avoid_: Script file, mutable processor

**Processor Binding**:
A subscription's selection of a Message Processor. Processing always uses the processor's current saved content.
_Avoid_: Script ID, decoder selection

**Processor Result**:
The structured value or failure produced by applying a specific Processor Revision to one MQTT message.
_Avoid_: Parsed string, script output

# MQTT Workbench

This context describes the language used to prepare and publish MQTT messages from the workbench.

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

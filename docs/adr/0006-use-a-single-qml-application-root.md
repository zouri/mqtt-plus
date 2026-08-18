# Use a single QML application root

- Status: Accepted
- Implementation: Implemented
- Date: 2026-08-17

## Context

QML needs feature state, models, and a small set of application workflow
commands. Routing every command through a feature ViewModel previously created
pass-through methods that duplicated an existing QObject interface without
hiding behavior. Exposing unrelated root objects separately, however, makes
startup wiring and page dependencies difficult to audit.

## Decision

Inject only `ApplicationViewModel` into QML as the `app` root property.
`ApplicationViewModel` may expose feature ViewModels, models, and selected
workflow QObjects whose `Q_PROPERTY` and `Q_INVOKABLE` surface is intentionally
stable for QML.

Create a feature ViewModel when it owns presentation state, translation,
selection, or an intent that combines multiple workflow calls. Do not add a
ViewModel method that only forwards one call to an already approved QML-facing
interface.

Pass the required interface from `Main.qml` into feature views explicitly. Do
not register application workflow objects as QML globals or inject additional
root properties.

Architecture tests enforce the single root and the approved exports directly;
they do not infer dependency policy from C++ class-name tokens appearing in QML.

## Consequences

- QML has one auditable application entry point.
- Feature ViewModels remain focused on presentation behavior instead of proxying
  every workflow command.
- Adding a new direct workflow export changes the application interface and must
  update the architecture test and this decision when the policy changes.
- Workflow QObjects exposed to QML must keep their invokable surface deliberate.

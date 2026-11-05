# Mission Objectives

Minimal, reusable objective tracking for SLAYCREW missions.

## Overview

`USlayObjectiveSubsystem` (`Source/SLAYCREW/Missions/`) is a `UWorldSubsystem`
created automatically in Game and PIE worlds. No map, GameMode, or player
Blueprint changes are required. Access it from:

- C++: `GetWorld()->GetSubsystem<USlayObjectiveSubsystem>()`
- Blueprint: the world subsystem getter node for *Slay Objective Subsystem*

## Lifecycle

```
Inactive --Activate--> Active --Complete / progress reaches target--> Completed
                          \----Fail----------------------------------> Failed
```

Completed and Failed are terminal. Invalid transitions return `false` and do
nothing (no events).

| Call | Effect |
| --- | --- |
| `RegisterObjective(Definition)` | Adds an objective (Inactive). Rejects None/duplicate IDs. Optionally auto-activates. |
| `ActivateObjective(Id)` | Inactive -> Active |
| `AddObjectiveProgress(Id, Delta)` | Positive delta on Active objectives; clamped; completes at target |
| `CompleteObjective(Id)` | Active -> Completed; fills progress to target |
| `FailObjective(Id)` | Active -> Failed |

Queries: `HasObjective`, `GetObjectiveStatus`, `GetObjectiveState`,
`GetObjectiveIdsInState`, `GetAllObjectives`.

## Events

- `OnObjectiveStateChanged(Id, PreviousState, NewState)` (Blueprint)
- `OnObjectiveProgressChanged(Id, Current, Target)` (Blueprint)
- `...Native` C++ multicast mirrors for non-UObject listeners

When progress reaches the target, the progress event fires before the
completion state event. Listeners may safely call back into the subsystem.

## Multiplayer

Mutating calls are ignored when the world's net mode is `NM_Client`, so
gameplay code can call them unconditionally and only the server/standalone
instance changes state. **State is not replicated yet**; clients see no
objectives. A future step is mirroring state through a replicated actor
(e.g., a GameState component) once the network architecture is agreed.

## Not included (by design)

Quest editor, data assets/mission graphs, objective dependencies, UI,
save/load, story content, and combat hooks. Integration (e.g., enemy kills
calling `AddObjectiveProgress`) is left to the owning systems.

## Tests

Automation test `SLAYCREW.Missions.ObjectiveLifecycle`
(`Source/SLAYCREW/Missions/Tests/SlayObjectiveSubsystemTests.cpp`).

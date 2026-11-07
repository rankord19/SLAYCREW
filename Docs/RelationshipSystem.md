# Relationship & Narrative System

C++ foundation for companion relationships, story decisions, conditional
events, romance routes and chapter-persistent narrative state. It does not
depend on combat or environment code, and it doesn't touch maps, GameModes or
character Blueprints.

Source: `Source/SLAYCREW/Narrative/`

| File | Purpose |
| --- | --- |
| `SlayNarrativeTypes.h` | Dimensions, romance states, conditions, custom-condition base class |
| `SlayRelationshipTypes.h` | Values, per-character dimension config, thresholds, romance rules, profiles, runtime records |
| `SlayNarrativeChoice.h` | Decision options (`FSlayNarrativeChoice`) and conditional events (`FSlayNarrativeEventDefinition`); both usable as DataTable rows |
| `SlayNarrativeSaveData.h` | Versioned save snapshot + standalone `USlayNarrativeSaveGame` |
| `SlayRelationshipProfileAsset.h` | Data asset holding profiles |
| `SlayNarrativeState.h/.cpp` | All logic: queries, mutations, conditions, choices, events, save/load |
| `SlayNarrativeSubsystem.h/.cpp` | Game-instance owner (persists across chapter maps) |
| `Tests/SlayNarrativeTests.cpp` | Automation tests `SLAYCREW.Narrative.*` |

## Concepts

### Relationships
A relationship is an **unordered pair** of character IDs. Companion routes pair
a companion with `Protagonist` (`USlayNarrativeState::GetProtagonistId()`).
Pairs between companions use the same structures (e.g. `CompanionA`/`CompanionB`).

Each relationship tracks four **independent** integer dimensions, which are
never combined into one score:

| Dimension | Meaning |
| --- | --- |
| `Trust` | Trust |
| `Friendship` | Friendship / emotional closeness |
| `Affection` | Romantic attraction / affection |
| `Tension` | Tension / conflict |

Each `FSlayRelationshipProfile` configures, per relationship:
- `Min`/`Max`/`Initial` per dimension, so relationships can start in different states.
- `GainPercent`/`LossPercent` per dimension, so the same choice can affect each character differently.
- `Thresholds`: fire `OnRelationshipThresholdCrossed` on crossing and can set a persistent relationship flag.
- `InitialFlags`, free-form `Metadata`.
- `bRomanceRouteAvailable` and `RomanceTransitions` (see below).

Character IDs, values and rules are content. The code defines no characters, and
the tests use placeholder IDs only.

### Flags, decisions, events, chapters
- **Global flags** (`SetFlag`/`HasFlag`) and **relationship flags**
  (`SetRelationshipFlag`) persist until explicitly cleared.
- **Decisions** store `DecisionId → OptionId`; by default a decision can only be made once.
- **Seen events** (`MarkEventSeen`) make one-shot events unavailable afterwards.
- **Chapter** is a forward-only integer (`AdvanceToChapter`). The index meaning is defined by content.

### Conditions
`FSlayNarrativeConditionSet` passes when every `AllOf` passes, at least one
`AnyOf` passes (if any are listed) and every `Custom` passes. Condition types:
`RelationshipValue`, `RomanceState`, `RomanceRouteAvailable`, `NarrativeFlag`,
`RelationshipFlag`, `Decision`, `EventSeen` and `Chapter`, plus `bNegate`.

For prerequisites the built-in types can't express, subclass
`USlayNarrativeCustomCondition` (C++ or Blueprint) and add it inline. It must be
deterministic and read-only.

### Choices
An `FSlayNarrativeChoice` bundles one option's consequences:
relationship changes on any number of characters, relationship flags, global
flags set or cleared, and romance transition requests. `ApplyChoice` is
**all-or-nothing**: if the requirements fail, a referenced relationship is
unknown, or a romance request isn't allowed, nothing changes. Effects run in
field order, and notifications are sent after the whole choice has applied,
in a deterministic order.

"Unlocking" a later conversation or personal event works like this: the choice
sets a flag, and the event's conditions require that flag.

### Romance
- A route exists only if `bRomanceRouteAvailable` is true. This is the character-specific boundary.
- Only transitions listed in the profile's `RomanceTransitions` are possible, so
  each companion can follow a different progression (e.g. no `Interested`
  stage, no reconciliation, or no rekindling after `Separated`).
- States: `None`, `Interested`, `Committed` (mutually agreed), `Strained`
  (conflict), `Separated`. Reconciliation means a `Strained → Committed` rule.
- **Numbers alone can't unlock romance.** Validation rejects any rule entering
  `Interested` or `Committed` unless its `AllOf` contains a non-negated story
  condition (flag, decision or seen event) or it has a custom condition.
- Romance changes only happen when they are requested, never automatically from
  value changes. Request them through a choice, so each step results from an
  explicit player decision, while the rule's conditions represent the companion's side.
- `Affection` and `Friendship` stay separate dimensions, so a relationship can
  grow as friendship without romance.

### Events (native + Blueprint)
`OnRelationshipValueChanged`, `OnRelationshipThresholdCrossed`,
`OnRomanceStateChanged`, `OnFlagChanged` (character IDs are `None` for global
flags), `OnChoiceApplied`, `OnChapterChanged` and `OnStateRestored`. Each has a
`...Native` C++ multicast mirror. Listeners always see a consistent state and
can safely call back into the system.

## Persistence
`ExportSaveData()` produces an `FSlayNarrativeSaveData` (version 1)
containing only state: relationship values, romance states and flags, global
flags, decisions (in order) and seen events. Name lists are sorted, so output
is deterministic. The chapter is stored as well. Configuration is not saved, so
content can change between updates.

`ImportSaveData()`:
- Rejects unsupported versions, a negative chapter, invalid or duplicate
  relationships, invalid romance states and duplicate decisions. **State is
  unchanged on failure.**
- Clamps out-of-range values to the current profile bounds and resets romance
  on routes that are now unavailable, reporting warnings in `OutMessages`.
- Gives relationships added after the save was written their profile defaults.
- Keeps entries for relationships without a profile, so later content can pick them up.
- Broadcasts `OnStateRestored` once (no per-value events).

Integration options:
- Embed `FSlayNarrativeSaveData` in the project's future `USaveGame`, or
- Use `USlayNarrativeSubsystem::SaveToSlot` / `LoadFromSlot` with the
  standalone `USlayNarrativeSaveGame` for now.

When the layout changes, bump `FSlayNarrativeSaveData::CurrentVersion` and add
a migration step at the marked spot in `ImportSaveData`.

## How chapters and dialogue scripts use it
1. At startup (e.g. in a GameInstance Blueprint or game code), register
   profiles: `GetSubsystem<USlayNarrativeSubsystem>()->RegisterProfileAsset(Asset)`.
2. Get the state: `GetNarrativeState()`. In Blueprint, use the
   *Slay Narrative Subsystem* getter, then *Get Narrative State*.
3. Dialogue and story scripts:
   - Show an option only if `IsChoiceAvailable(Choice)`; on selection call `ApplyChoice(Choice)`.
   - Gate lines, conversations and story beats with `EvaluateConditionSet` or
     `IsEventAvailable`; after playing a one-shot event, call `MarkEventSeen`.
   - Store choices and events in DataTables (row structs `FSlayNarrativeChoice`,
     `FSlayNarrativeEventDefinition`) or in any data asset.
4. At chapter transitions call `AdvanceToChapter(N)` and save. The state lives in
   a game-instance subsystem, so it survives map travel within a session.
5. UI or presentation layers bind to the events; they never own state.

## Multiplayer
The state isn't replicated. It is designed to be changed only by the
authoritative/local game instance. Co-op and PvP scope isn't decided yet, so
replication is left for later.

## Not included
No dialogue editor, UI, dialogue text, story content, character definitions,
cutscenes or combat integration.

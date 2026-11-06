# Combat, protagonist death and companion injury foundation

## Branch and prerequisites

Implemented on `codex/astra-combat-injury`, a fresh branch containing PR #1's
health commit `788d831`. Main was `d7c51d2` when fetched on 2026-11-07.
PRs #1, #3 and #4 were open and unmerged. This PR targets main and therefore
includes #1 until it is merged. No main merge, Opus/Argon edits, maps or binary
assets are part of this work. PR #3 is an optional content integration, not a
compile dependency. PR #4 is unrelated.

## Enable the placeholder player

Reparent the existing third-person character Blueprint to `ASlayFighterCharacter`
in a content integration change. It inherits the current camera, movement and
Enhanced Input configuration. Do not add a second health component. Existing
maps/Blueprints are intentionally unchanged, so they do not automatically adopt
this new class. Alternatively spawn/possess the native fighter for code testing;
it needs a mesh/animation Blueprint for visual presentation.

With no BasicAttackAction asset assigned, SetupPlayerInputComponent installs a
transient Enhanced Input mapping for left mouse button and gamepad right shoulder.
An assigned action instead uses the controller's existing mapping contexts. Input
uses Started (one request per press). `BasicAttack` also supports UI controls.
Mapping contexts are removed on unpossession and EndPlay.

The only template class edit adds FObjectInitializer forwarding so derived fighters
can install USlayCharacterMovementComponent. Other template variants keep their
existing behavior and HP implementations; do not mix Variant_Combat's legacy HP
with the new foundation on the same actor.

## Combat contract

USlayMeleeComponent requires exactly one USlayHealthComponent on its owner. The
owner must replicate and a requesting client must own the actor. RequestAttack
routes to a server RPC; TryStartAttack checks authority, HP, state and cooldown.
No client-supplied target, position, damage or timing is trusted. AI can call
TryStartAttack directly on authority.

Defaults: 25 damage, 150 cm reach, 35 cm radius, 0.15 second active window, then
0.45 second cooldown. A forward sphere sweep at attack start and each active
server tick queries Pawn and WorldDynamic bodies. A Visibility trace to the
target origin rejects occluded actors. Targets need query collision and a health
component; walls must block Visibility. Each actor is recorded before damage is
dispatched, so multiple primitives and later sweeps cannot hit it twice in one
attack. Standard Unreal ApplyDamage routes through the existing health component
and preserves instigator/causer for listeners. There is no second HP or armor pool.

Attacking -> Cooldown -> Idle is server-owned and replicated. Cancellation ends
the active window but retains its original cooldown; zero HP cancels immediately.
Wounded attack speed divides both active and recovery durations when an attack
starts. Changes during an attack affect the next attack; incapacitation always
cancels. OnAttackStateChanged supports placeholder animation/audio. Override
CanHitTarget for future team/PvP rules or weapon filters. Default behavior permits
any other damageable actor with health, including companions: no faction design
is assumed. Future armor mitigation belongs in the single damage path.

This is a short forward volume, not a weapon-socket or animation-notify system.
It does not sweep the space traveled between ticks, provide lag compensation,
combos, hit reactions, weapons, or client-side prediction. Occlusion to target
origins is deliberately conservative around partial cover.

## Death and companion states

The protagonist uses normal terminal health: zero HP cancels combat, stops jump
and movement, disables character movement, and gates movement/look/jump/attack
entry points. OnProtagonistDeath and ReceiveProtagonistDeath fire once locally for
death animation, sound and Game Over UI. Gate local UI by IsLocallyControlled.
No destruction, save deletion, respawn or story outcome is implemented.

Use `ASlayCompanionCharacter` for each of the four companions. It sets the same
health component's bIncapacitateAtZero policy in its constructor. At zero HP,
IsDepleted is true but IsDead is false; OnDeath is not broadcast. Consumers that
ask whether an actor can act must use IsDepleted/CanPerformActions, not IsDead.
Configure the policy in defaults, never change it while playing.

USlayInjuryComponent derives all state from current/max health:

| HP fraction | State | Behavior |
| --- | --- | --- |
| > 0.5 | Healthy | Full performance |
| > 0 and <= 0.5 | Wounded | Conscious, action-capable, progressively slower |
| 0 | Incapacitated | No movement, attacks, dodge or abilities |

Percentages are continuous: 50.1% is Healthy; 50% is Wounded. Wound severity is
clamp(1 - HP fraction / 0.5, 0, 1), so modifiers start continuously at full speed
at the 50% boundary and approach configurable minima near zero. Default minimum
movement/attack/dodge multipliers are 0.4 / 0.5 / 0.25. At zero, all are zero.
The movement component multiplies Super::GetMaxSpeed without overwriting base
stats. Healing above half restores full performance. Movement stops immediately
on depletion; the movement tick discards input/forces while depleted.

OnInjuryChanged fires for every HP change, including within Wounded; bind wounded
locomotion, hit reaction, breathing, audio and animation changes here.
OnIncapacitated is the collapse presentation hook. Future dodge/ability code MUST
check CanPerformActions/CanAct on authority and consume GetDodgeEffectivenessMultiplier.
There is no existing dodge/ability framework to modify or cancel in this scope.

Ordinary healing cannot leave incapacitation by default. Authority can explicitly
call RecoverFromIncapacitation(HealthFraction), or a future recovery design may
opt in to bAllowHealingRecovery. Explicit recovery only works at zero for companions;
it cannot resurrect a protagonist. Positive fractions clamp to [0.001, 1].
Recovery restores actions and derives movement from current health without
resetting base stats or bypassing an outstanding combat cooldown.

## Optional PR #3 integration

PR #3 provides ASLAYCREWEnemyCharacter::ReceiveAttackIntent and the behavior
component's OnAttackIntent. After #3 is integrated, add one health and one melee
component in a derived enemy Blueprint. Route that existing attack-intent hook to
TryStartAttack on authority. Facing remains the AI controller's responsibility.
Bind health OnDeath to stop that enemy's AI through its existing control hooks.
The melee component discovers health by type and makes no assumption about the
Argon character class. This integration is documented, not compiled or tested in
this branch; no duplicate enemy behavior framework was added.

## Networking and verification limits

Health and attack state replicate on replicating owners. Injury modifiers are
derived locally from replicated health; configure identical component defaults
on server/client. Replication may coalesce short transitions and is not an event
history. Health maximum/policy/current values are separate properties; initial
presentation should query getters after initialization. Wake dormant actors before
mutating them if dormancy is introduced. The supplied fighters do not enable it.

Server authority checks are implemented, but multiplayer PIE/dedicated-server,
packet loss, late joining, prediction correction and hostile RPC tests have not
been run. No claim of verified network correctness is made. In particular future
predicted companion movement needs saved-move support for injury changes. Test
client input, server damage, one hit per target, remote death/incapacitation,
recovery and unpossession with two clients before multiplayer use.

Automation names:
- SLAYCREW.Health.BoundsAndDamage
- SLAYCREW.Combat.SweepCooldownAndDeath
- SLAYCREW.Combat.CompanionInjuryAndRecovery
- SLAYCREW.Combat.WallsRangeAndCancellation

Build/test results are recorded below after execution.

## Executed results (2026-11-07, Asia/Baku)

- UE 5.8.4 `SLAYCREWEditor Win64 Development`: succeeded.
- All four tests above passed in UnrealEditor-Cmd with NullRHI: report records
  succeeded=4, failed=0, succeededWithWarnings=0; process exit code 0.
- `git diff --check`: passed.
- Initial parallel build exhausted system commit memory; building with
  `-MaxParallelActions=1` succeeded. MSVC 14.52.37025 is newer than UE's preferred
  toolchain and emits a warning.
- Initial test fixtures lacked a world context, complete actor/physics lifecycle,
  advancing frame counter and authority GameMode. These fixture defects were
  corrected before the passing run. Assertions were retained, including real
  collision queries and Unreal pawn damage routing.
- No multiplayer, visual PIE, hardware input, packaged-game or PR #3 integration
  testing was performed. Blueprint reparenting and presentation remain content
  integration steps; no shared binary files were edited.

Reproduction (PowerShell, adjust the engine path if necessary):

```powershell
& 'D:/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' SLAYCREWEditor Win64 Development '-Project=D:/GitHub/SLAYCREW-Astra/SLAYCREW.uproject' -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=1
& 'D:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/GitHub/SLAYCREW-Astra/SLAYCREW.uproject' -unattended -nop4 -NullRHI -nosound '-ExecCmds=Automation RunTests SLAYCREW.' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/GitHub/SLAYCREW-Astra/Saved/Automation/Combat' '-abslog=D:/GitHub/SLAYCREW-Astra/Saved/Logs/CombatAutomation.log'
```

Read the JSON report rather than relying solely on process exit status: the
Unreal test-queue-empty exit condition can also exit zero with failed assertions.
Local evidence is in Saved/Automation/Combat/index.json and
Saved/Logs/CombatAutomation.log (generated files are intentionally ignored).

## Source inventory

New files:
- Source/SLAYCREW/Combat/SlayMeleeComponent.h and .cpp
- Source/SLAYCREW/Combat/SlayInjuryComponent.h and .cpp
- Source/SLAYCREW/Combat/SlayCharacterMovementComponent.h and .cpp
- Source/SLAYCREW/Combat/SlayFighterCharacter.h and .cpp (includes companion subclass)
- Source/SLAYCREW/Tests/SlayCombatTests.cpp
- Source/SLAYCREW/Tests/SlayCombatTestObserver.h
- Docs/CombatAndInjury.md

Modified files:
- Source/SLAYCREW/Components/SlayHealthComponent.h and .cpp
- Source/SLAYCREW/SLAYCREWCharacter.h and .cpp (constructor forwarding only)
- Docs/HealthComponent.md

# First playable combat prototype

Branch: `codex/astra-playable-combat`, based on `codex/astra-combat-injury`
(`6c5a838`). Draft PR targets that branch, not main. PR #3 is not integrated.

## Launch in Unreal Engine 5.8.4

1. Build `SLAYCREWEditor Win64 Development`, then open this worktree's
   `SLAYCREW.uproject` in UE 5.8.4.
2. Save any edited level. In the editor's bottom **Cmd** console, execute
   `Slay.Prototype`. It replaces the current editor map with a temporary empty
   map, assigns `SlayPrototypeGameMode`, and starts Play In Editor. It refuses
   to replace a dirty loaded level or an already running PIE session.
3. Click the viewport to capture input. WASD moves; mouse looks; Shift sprints;
   Space jumps. The enemy is stationary, facing the initial player position.
4. Walk toward the enemy and face it using movement. Hold LMB, move the mouse,
   release to strike. A quick click defaults to Right. The camera is held while
   selecting the opening. Only the opening uses mouse direction.
5. Press LMB near the end of recovery to queue the next strike. Inputs expire
   after 0.3 seconds; the follow-up window lasts 0.2 seconds after full recovery.
   Maximum three strikes. Stop pressing to stop the combo. Stamina is charged
   once at accepted startup, never when merely queuing input.
6. Stand in front of the enemy to receive timed attacks. At zero HP the existing
   protagonist death path disables actions/movement and the HUD shows GAME OVER.
7. Esc stops PIE; press Play to restart on the same temporary arena. No map needs
   saving. Stop PIE before returning to the normal project map.

Alternative: create an Empty level, select `SlayPrototypeGameMode` in World
Settings -> GameMode Override, and press Play. Use one player, **Standalone** net
mode. The game mode generates the bounded floor/walls, lighting and timed enemy
at runtime; existing maps, input assets and Blueprints are unchanged.

## Implemented

- Native player derives from the existing fighter: Health, Melee, Injury and
  protagonist death are reused. Existing mannequin mesh and locomotion AnimBP
  are loaded from project assets. Existing combat-group/friendly-fire rules,
  wall/range checks and per-attack hit deduplication remain in the melee sweep.
- Optional configurable melee startup preserves old behavior at its zero default.
  Prototype uses 0.18s startup, 0.18s active, 0.32s recovery. Injury still scales
  attack timing. Last accepted FHitResult is retained for future hit zones.
- Reusable authority-owned stamina with replicated current value, max, delayed
  regeneration, finite cost validation and no overdraft. Prototype light attacks
  cost 28/100; sprint costs 22/s regardless of combat. Walking costs nothing.
- Finite combo component: expiring single input buffer, four opening directions,
  configurable gesture threshold, and clearing on death/cancellation/unpossession.
- Light-attack movement/turn limits (140 cm/s and 90 degrees/s before injury),
  normal camera-relative movement, no target snapping or aim assistance.
- HUD shows HP, stamina, enemy HP, attack phase, opening direction, combo index,
  accepted hit count, insufficient stamina, enemy defeat and game over.

## Deliberate limits

- Light attacks only. Heavy attacks, dodge and their separate cost/commitment
  rules are deferred. Sprint is implemented and charged inside/outside combat.
- Sword is a collisionless cube animated procedurally beside the mannequin.
  There are no authored directional sword animations, hand grip or attack
  montages. Existing unarmed attack montages were not treated as sword assets.
  Damage uses the inherited forward sphere sweep, not the displayed blade path.
  Direction selects presentation/state; later combo strikes repeat the temporary
  swing, not a polished animation sequence. No new binary assets were created.
- Prototype hardware input, combo state and HUD are standalone only; the harness
  deliberately does not accept remote-client input. Base health/melee authority
  is preserved; networked combos, prediction and replication presentation are
  deferred. Stamina defaults must match peers if reused elsewhere.
- Enemy is a fixed timed attacker, not Argon's AI. No chase, lock-on or turning
  toward the player. Restart PIE to reset the scenario; no in-game respawn.
- Cancellation ends an active/startup window and clears combo input, while
  retaining the original cooldown. No stamina refund after an executed startup.
- The launcher is compiled only in editor targets (UnrealEd/LevelEditor).

## Verification actually executed

2026-11-07 (Asia/Baku): UE **5.8.4**, Editor Win64 Development build succeeded.
Build used `-WaitMutex -NoHotReloadFromIDE -MaxParallelActions=1`. MSVC
14.52.37025 emits UE's warning that it is newer than the preferred toolchain.
An initial compile error in the stamina replication macro was corrected.
The first sandboxed SDK launch could not run; the authorized external build ran.

Final UnrealEditor-Cmd NullRHI run: **11 succeeded, 0 failed, 0 warnings** in
`Saved/Automation/PlayableFinal/index.json` (generated/ignored). Exact tests:

- SLAYCREW.Health.BoundsAndDamage
- SLAYCREW.Combat.CompanionInjuryAndRecovery
- SLAYCREW.Combat.MovementLockAndRecovery
- SLAYCREW.Combat.SweepCooldownAndDeath
- SLAYCREW.Combat.TargetEligibility
- SLAYCREW.Combat.WallsRangeAndCancellation
- SLAYCREW.Prototype.StaminaAndRejection
- SLAYCREW.Prototype.BufferAndFiniteCombo
- SLAYCREW.Prototype.BufferExpiryAndDeath
- SLAYCREW.Prototype.OpeningDirections
- SLAYCREW.Prototype.StartupAndDamageExchange

Run with:

```powershell
& 'D:/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'D:/GitHub/SLAYCREW-Astra/SLAYCREW.uproject' -unattended -nop4 -NullRHI -nosound '-ExecCmds=Automation RunTests SLAYCREW.' '-TestExit=Automation Test Queue Empty' '-ReportExportPath=D:/GitHub/SLAYCREW-Astra/Saved/Automation/PlayableFinal'
```

Actual graphical PIE was launched through `Slay.Prototype` and inspected through
Windows computer use. Observed the rendered arena, both mannequins/swords,
movement/camera changes, combo/stamina changes and damage in both directions
(enemy 50 HP, player 40 HP, two accepted player hits). Sent a quick-click attack
through the viewport and observed startup. This is a limited interactive smoke
check, not exhaustive hardware-input verification. Death/action lock is covered
by automation; visible game-over, all gesture directions, sprint exhaustion and
precise buffer timing still warrant manual acceptance using the steps above.
No multiplayer or packaged build was tested. `git diff --check` passed.

## Files

Created:
- Source/SLAYCREW/Components/SlayStaminaComponent.h and .cpp
- Source/SLAYCREW/Combat/SlayComboComponent.h and .cpp
- Source/SLAYCREW/Prototype/SlayPrototype.h and .cpp
- Source/SLAYCREW/Prototype/SlayPrototypeEditor.cpp
- Docs/PlayableCombat.md

Modified:
- Source/SLAYCREW/Combat/SlayMeleeComponent.h and .cpp
- Source/SLAYCREW/Tests/SlayCombatTests.cpp
- Source/SLAYCREW/SLAYCREW.Build.cs

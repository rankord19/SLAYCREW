# Health component

Add `USlayHealthComponent` (`Components/SlayHealthComponent.h`) as a default
subobject or Blueprint component on an actor. No existing actors are changed.
Configure MaxHealth in defaults; authority starts at full health in BeginPlay.
Non-finite capacity falls back to 100; finite capacity is clamped to at least 1.
Current health stays between zero and capacity. Runtime capacity changes and
revival are intentionally not provided.

Call ApplyDamage or Heal on authority after BeginPlay; each returns the actual
amount removed/restored. Non-positive and non-finite inputs are ignored. Healing
cannot revive a dead actor. The component also binds the owner's OnTakeAnyDamage,
so Unreal ApplyDamage/point/radial damage works when the actor permits damage and
its TakeDamage override preserves the engine event. Use one damage path per hit
and one health component per actor to avoid duplicate application.

OnHealthChanged reports previous/new health; OnDeath fires on a positive-to-zero
transition. Death does not destroy the actor, disable movement, or play animation.
Bind presentation/gameplay as needed; query getters when initially binding UI.
Before BeginPlay / initial replication, getters may expose constructor defaults.

## Networking assumptions

- Standalone and server actor authority may mutate health. Both C++ and Blueprint
  calls are guarded at runtime; client calls do nothing, and no RPC is exposed.
  Future input handling must validate requests on the server before applying damage.
- The component replicates by default, but the owning actor must also replicate.
  Prefer a default subobject; dynamic component creation must occur on the server
  with Unreal's replicated component lifecycle. Client-only actors are not a
  supported source of shared gameplay state.
- Maximum and current health replicate. OnRep sends local change/death notifications;
  authority sends them directly. Replication can coalesce intermediate changes and
  is not an event history. A late observer of a dead actor can receive OnDeath.
  Authoritative death consequences belong on the server, not in client callbacks.
- Owner relevancy, dormancy and update frequency still govern delivery. Code that
  modifies a dormant actor must wake/flush it as appropriate. No prediction,
  resurrection, team rules, damage attribution, or combat framework is included.
- Maximum and current health are separate replicated properties; callbacks should
  not assume cross-property notification order when initializing UI.

## Verification

Automation test: `SLAYCREW.Health.BoundsAndDamage` covers engine damage routing,
initial health, invalid inputs, overflow-sized healing/damage, and terminal death.
Run with the Editor automation runner. Multiplayer PIE validation still needs a
replicating test actor: damage/heal on server, verify client getters/notifications,
reject client mutations, and connect a late client after death. Also check custom
maximum defaults (including invalid values) and count death notifications once.


# SLAYCREW — Development Instructions

## Project Overview

SLAYCREW is a story-driven, third-person action game
developed in Unreal Engine 5 using C++ and Blueprints.

The story follows five newly graduated monster slayers:
one playable protagonist and four close companions
(two women and two men).

The game emphasizes grounded environments, immersive
storytelling, responsive combat, and handcrafted locations.

## Core Design

- Third-person camera with fully controllable movement.
- Story-driven missions within bounded locations.
- Five central characters with persistent relationships.
- Real-time PvE monster combat.
- PvP and cooperative multiplayer are planned features;
  their exact scope is not finalized.
- Avoid creating an open world.
- Aim for realistic environments and coherent art direction.
- Use placeholder assets where appropriate.

## Technical Principles

- C++ for reusable gameplay systems.
- Blueprints for content configuration and presentation.
- Use Unreal Engine's existing gameplay framework.
- Design authority-sensitive gameplay with multiplayer
  in mind; do not assume a full network implementation
  already exists.
- Avoid unnecessary dependencies.
- Preserve existing working functionality.
- Prefer small, testable, modular implementations.
- Never claim successful compilation without running it.

## Agent Collaboration

Each agent works in its assigned Git worktree:

Astra:
D:\GitHub\SLAYCREW-Astra
Branch: agent/astra-core

Opus:
D:\GitHub\SLAYCREW-Opus
Branch: agent/opus-gameplay

Argon:
D:\GitHub\SLAYCREW-Argon
Branch: agent/argon-world

Agents must:
1. Read this file before modifying anything.
2. Inspect the existing Unreal project structure.
3. Work only within their assigned worktree.
4. Never directly modify main.
5. Never overwrite another agent's work.
6. Avoid shared .umap and .uasset edits.
7. Keep changes focused on the assigned task.
8. Report blockers instead of endlessly retrying.
9. Commit changes to their own branch.
10. Push and open a PR targeting main.
11. Never merge without user approval.

## Initial Responsibilities

Astra:
Core character systems, health, damage, combat architecture.

Opus:
Mission objectives, interactions, progression architecture.

Argon:
Monster AI, enemy behavior, encounter architecture.

Responsibilities can change between milestones.

## Completion Requirements

Every implementation must report:

- Summary of changes
- Files modified or created
- Compilation and tests actually performed
- Known issues and unfinished work
- Commit hash
- Pull request URL

## Working Agreement

Do not independently invent major story decisions,
characters, game modes, or monetization systems.

Do not implement large features outside the task scope.

When requirements are unclear, favor a minimal,
documented implementation over speculation.

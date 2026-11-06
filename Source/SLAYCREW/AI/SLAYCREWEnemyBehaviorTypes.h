// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SLAYCREWEnemyBehaviorTypes.generated.h"

class AActor;

/**
 * Reusable interface for actors or components that can report whether they are currently
 * combat-capable and eligible to be targeted by enemy AI (e.g., not dead and not incapacitated).
 *
 * Integration points supported by USLAYCREWEnemyBehaviorComponent::IsTargetEligible:
 * 1. Implementing ISLAYCREWCombatTargetInterface on the target Actor or any of its ActorComponents.
 * 2. Binding USLAYCREWEnemyBehaviorComponent::CustomTargetEligibilityPredicate.
 * 3. Zero-argument UFUNCTIONs on the target Actor or its components (e.g. USlayHealthComponent / USlayInjuryComponent):
 *    IsDead(), IsIncapacitated(), IsDepleted(), IsAlive(), CanAct(), CanPerformActions(), GetCurrentHealth().
 * 4. Actor or Component tags: "Dead", "Incapacitated", "Downed", "Untargetable".
 */
UINTERFACE(MinimalAPI, Blueprintable)
class USLAYCREWCombatTargetInterface : public UInterface
{
	GENERATED_BODY()
};

class SLAYCREW_API ISLAYCREWCombatTargetInterface
{
	GENERATED_BODY()

public:
	/**
	 * Returns true if this actor/component is alive, conscious, and capable of combat.
	 * Protagonists at 0 HP (dead) and companions at 0 HP (incapacitated) should return false.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SLAYCREW|AI")
	bool IsTargetCombatCapable() const;
	virtual bool IsTargetCombatCapable_Implementation() const { return true; }
};

/** Optional C++ predicate for custom target-eligibility overrides. */
DECLARE_DELEGATE_RetVal_OneParam(bool, FSLAYCREWTargetEligibilityPredicate, const AActor* /*CandidateTarget*/);

/**
 * High-level behavior states for SLAYCREW enemy AI.
 * Kept intentionally decoupled from concrete character, health, and combat classes.
 */
UENUM(BlueprintType)
enum class ESLAYCREWEnemyBehaviorState : uint8
{
	/** Enemy is stationary and awaiting stimuli or patrol timer expiration. */
	Idle UMETA(DisplayName = "Idle"),

	/** Enemy is moving along configured patrol waypoints. */
	Patrol UMETA(DisplayName = "Patrol"),

	/** Enemy has acquired a target and is actively closing distance. */
	Chase UMETA(DisplayName = "Chase"),

	/** Enemy is within engagement range and signaling placeholder intent to attack. */
	AttackIntent UMETA(DisplayName = "Attack Intent")
};

/**
 * Tunable configuration parameters governing enemy state transitions.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSLAYCREWEnemyBehaviorConfig
{
	GENERATED_BODY()

	/** Time in seconds the enemy stays in Idle before transitioning to Patrol (when patrol points exist). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Idle", meta = (ClampMin = "0.0", Units = "s"))
	float IdleDuration = 2.0f;

	/** Distance threshold in cm to consider a patrol waypoint reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Patrol", meta = (ClampMin = "10.0", Units = "cm"))
	float PatrolAcceptanceRadius = 100.0f;

	/** If true, patrol waypoints wrap around after reaching the last point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Patrol")
	bool bLoopPatrol = true;

	/** If true, reaching a patrol waypoint briefly transitions to Idle before advancing to the next waypoint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Patrol")
	bool bPauseAtPatrolWaypoints = true;

	/** Distance in cm within which a valid target causes a transition into Chase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Chase", meta = (ClampMin = "0.0", Units = "cm"))
	float ChaseDetectionRange = 1000.0f;

	/** Distance in cm beyond which an active target is considered lost, returning to Idle/Patrol. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|Chase", meta = (ClampMin = "0.0", Units = "cm"))
	float LoseTargetRange = 1400.0f;

	/** Distance in cm within which a chased target causes a transition into AttackIntent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|AttackIntent", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackIntentRange = 220.0f;

	/** Distance in cm beyond which the enemy leaves AttackIntent and returns to Chase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|AttackIntent", meta = (ClampMin = "0.0", Units = "cm"))
	float AttackIntentDisengageRange = 280.0f;

	/** Interval in seconds between placeholder attack intent notifications while in AttackIntent state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior|AttackIntent", meta = (ClampMin = "0.1", Units = "s"))
	float AttackIntentInterval = 1.5f;
};

/** Broadcast when the enemy transitions from one behavior state to another. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSLAYCREWEnemyBehaviorStateChanged,
	ESLAYCREWEnemyBehaviorState, PreviousState,
	ESLAYCREWEnemyBehaviorState, NewState);

/** Broadcast when the active patrol waypoint index or destination changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSLAYCREWEnemyPatrolWaypointChanged,
	int32, NewWaypointIndex,
	FVector, NewWaypointLocation);

/** Broadcast when the enemy signals placeholder attack intent against its current target. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSLAYCREWEnemyAttackIntent,
	AActor*, TargetActor);

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/SLAYCREWEnemyBehaviorTypes.h"
#include "SLAYCREWEnemyBehaviorComponent.generated.h"

/**
 * Modular, authority-aware enemy behavior state machine component.
 * Manages transitions between Idle, Patrol, Chase, and AttackIntent states
 * without tightly coupling to concrete character, health, or damage classes.
 */
UCLASS(ClassGroup = (SLAYCREW), meta = (BlueprintSpawnableComponent))
class SLAYCREW_API USLAYCREWEnemyBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USLAYCREWEnemyBehaviorComponent();

	// ~Begin UActorComponent Interface
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// ~End UActorComponent Interface

	/** Evaluates state transitions and per-state timers on the authoritative instance. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void EvaluateBehavior(float DeltaTime);

	/**
	 * Pure state transition evaluation given current state and environment metrics.
	 * Exposed for deterministic unit testing and external queries.
	 */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	ESLAYCREWEnemyBehaviorState DetermineDesiredState(
		ESLAYCREWEnemyBehaviorState InCurrentState,
		bool bHasValidTarget,
		float DistanceToTarget,
		bool bHasPatrolPoints,
		float InTimeInState) const;

	/** Forces a state transition (authority only when owned by an actor). Returns true if state changed. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	bool SetBehaviorState(ESLAYCREWEnemyBehaviorState NewState);

	/** Returns the current active behavior state. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	ESLAYCREWEnemyBehaviorState GetCurrentState() const { return CurrentState; }

	/** Returns the elapsed time in seconds spent in the current behavior state. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	float GetTimeInCurrentState() const { return TimeInCurrentState; }

	/**
	 * Evaluates whether CandidateTarget is valid, alive, conscious (not incapacitated), and combat-capable.
	 * Checks CustomTargetEligibilityPredicate, ISLAYCREWCombatTargetInterface, disqualifying tags
	 * ("Dead", "Incapacitated", "Downed", "Untargetable"), and reflection hooks on the actor and its components
	 * (IsDead, IsIncapacitated, IsDepleted, IsAlive, CanAct, CanPerformActions, GetCurrentHealth).
	 */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	bool IsTargetEligible(const AActor* CandidateTarget) const;

	/** Returns true if CurrentTarget is assigned and currently passes IsTargetEligible(). */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	bool HasValidCombatTarget() const;

	/**
	 * Assigns the target actor to pursue/engage (authority only when owned by an actor).
	 * If NewTarget is not combat-eligible (e.g. dead or incapacitated), CurrentTarget is cleared.
	 */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void SetTargetActor(AActor* NewTarget);

	/** Clears the current target actor. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void ClearTargetActor();

	/** Returns the currently tracked target actor, if any. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	AActor* GetTargetActor() const { return CurrentTarget; }

	/** Replaces the patrol waypoint list (world-space locations) and resets the patrol index. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void SetPatrolWaypoints(const TArray<FVector>& InWaypoints);

	/** Appends a single world-space patrol waypoint. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void AddPatrolWaypoint(const FVector& Waypoint);

	/** Returns true if at least one patrol waypoint is configured, outputting the active waypoint location. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	bool GetCurrentPatrolWaypoint(FVector& OutWaypoint) const;

	/** Returns the index of the active patrol waypoint, or INDEX_NONE if no waypoints are available. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	int32 GetCurrentPatrolWaypointIndex() const { return CurrentPatrolIndex; }

	/** Advances to the next patrol waypoint according to loop configuration. Returns true if a valid next waypoint exists. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	bool AdvancePatrolWaypoint();

	/** Returns true if the component has at least one remaining valid patrol waypoint to visit. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	bool HasActivePatrolWaypoints() const;

	/** Triggers placeholder attack intent logic and broadcasts OnAttackIntent if HasValidCombatTarget() is true. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void TriggerAttackIntent();

public:
	/** Configuration parameters for state transitions and ranges. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI")
	FSLAYCREWEnemyBehaviorConfig BehaviorConfig;

	/** World-space patrol waypoints visited during the Patrol state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI")
	TArray<FVector> PatrolWaypoints;

	/** Optional C++ delegate to override or extend target eligibility rules. */
	FSLAYCREWTargetEligibilityPredicate CustomTargetEligibilityPredicate;

	/** Broadcast whenever the behavior state changes. */
	UPROPERTY(BlueprintAssignable, Category = "SLAYCREW|AI|Events")
	FOnSLAYCREWEnemyBehaviorStateChanged OnBehaviorStateChanged;

	/** Broadcast whenever the active patrol waypoint index or destination changes. */
	UPROPERTY(BlueprintAssignable, Category = "SLAYCREW|AI|Events")
	FOnSLAYCREWEnemyPatrolWaypointChanged OnPatrolWaypointChanged;

	/** Broadcast when placeholder attack intent is triggered in the AttackIntent state. */
	UPROPERTY(BlueprintAssignable, Category = "SLAYCREW|AI|Events")
	FOnSLAYCREWEnemyAttackIntent OnAttackIntent;

protected:
	/** Returns true if the component has authority to mutate state (or has no outer actor during isolated tests). */
	bool HasBehaviorAuthority() const;

	/** Replication callback for CurrentState. */
	UFUNCTION()
	void OnRep_CurrentState(ESLAYCREWEnemyBehaviorState PreviousState);

	/** Internal helper when entering a new state. */
	void OnEnterState(ESLAYCREWEnemyBehaviorState PreviousState, ESLAYCREWEnemyBehaviorState NewState);

private:
	/** Current behavior state, replicated from authority to clients. */
	UPROPERTY(ReplicatedUsing = OnRep_CurrentState, VisibleInstanceOnly, Category = "SLAYCREW|AI")
	ESLAYCREWEnemyBehaviorState CurrentState = ESLAYCREWEnemyBehaviorState::Idle;

	/** Current target actor being chased or engaged, replicated from authority to clients. */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "SLAYCREW|AI")
	TObjectPtr<AActor> CurrentTarget = nullptr;

	/** Index into PatrolWaypoints for the current destination. */
	UPROPERTY(VisibleInstanceOnly, Category = "SLAYCREW|AI")
	int32 CurrentPatrolIndex = INDEX_NONE;

	/** Elapsed time in seconds spent in CurrentState. */
	float TimeInCurrentState = 0.0f;

	/** Cooldown timer tracking placeholder attack intent pulses while in AttackIntent. */
	float AttackIntentTimer = 0.0f;
};

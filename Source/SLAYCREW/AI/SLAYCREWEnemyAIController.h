// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/SLAYCREWEnemyBehaviorTypes.h"
#include "Navigation/PathFollowingComponent.h"
#include "SLAYCREWEnemyAIController.generated.h"

class USLAYCREWEnemyBehaviorComponent;

/**
 * Minimal AI Controller that drives navigation and focus based on a possessed pawn's
 * USLAYCREWEnemyBehaviorComponent (Idle, Patrol, Chase, AttackIntent).
 * Avoids redundant per-tick MoveToLocation calls and skips dead or incapacitated targets.
 */
UCLASS()
class SLAYCREW_API ASLAYCREWEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASLAYCREWEnemyAIController();

	// ~Begin AController / AAIController Interface
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	// ~End AController / AAIController Interface

	/** Returns the behavior component cached from the currently possessed pawn. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	USLAYCREWEnemyBehaviorComponent* GetEnemyBehaviorComponent() const { return CachedBehaviorComponent; }

	/** Registers an additional candidate target actor (e.g., companion or party member) for auto-detection. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void RegisterCandidateTarget(AActor* CandidateActor);

	/** Unregisters a previously registered candidate target actor. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void UnregisterCandidateTarget(AActor* CandidateActor);

	/** Marks the active patrol move request as interrupted/failed so the controller recovers on the next retry window. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void NotifyPatrolMovementInterrupted();

	/** Returns the total number of patrol MoveToLocation requests issued by this controller (used for diagnostics/tests). */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	int32 GetPatrolMoveRequestCount() const { return PatrolMoveRequestCount; }

	/** Resets the diagnostic patrol MoveToLocation request counter. */
	UFUNCTION(BlueprintCallable, Category = "SLAYCREW|AI")
	void ResetPatrolMoveRequestCount() { PatrolMoveRequestCount = 0; }

public:
	/**
	 * If true, performs proximity and eligibility checks against player-controlled pawns
	 * and registered candidate targets when no valid combat target is currently assigned.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI")
	bool bAutoDetectPlayerTargets = true;

	/** Acceptance radius in cm passed to MoveToActor while in the Chase state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI", meta = (ClampMin = "10.0", Units = "cm"))
	float ChaseMoveAcceptanceRadius = 120.0f;

	/** Cooldown in seconds before re-issuing a patrol MoveToLocation after an interrupted or failed move. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI", meta = (ClampMin = "0.05", Units = "s"))
	float PatrolMoveRetryInterval = 0.5f;

protected:
	/** Responds to state transitions emitted by the possessed pawn's behavior component. */
	UFUNCTION()
	void HandleBehaviorStateChanged(ESLAYCREWEnemyBehaviorState PreviousState, ESLAYCREWEnemyBehaviorState NewState);

	/** Responds to patrol waypoint changes emitted by the behavior component. */
	UFUNCTION()
	void HandlePatrolWaypointChanged(int32 NewWaypointIndex, FVector NewWaypointLocation);

	/** Responds to placeholder attack intent events emitted by the behavior component. */
	UFUNCTION()
	void HandleAttackIntent(AActor* TargetActor);

	/** Applies movement and focus commands matching the active behavior state. */
	void ApplyStateMovementCommand(ESLAYCREWEnemyBehaviorState State);

	/** Issues a patrol MoveToLocation request only when entering Patrol, changing waypoints, or recovering from interruption. */
	void UpdatePatrolNavigation(float DeltaSeconds);

	/** Issues a single MoveToLocation request for the specified patrol waypoint and updates tracking state. */
	void IssuePatrolMoveRequest(int32 WaypointIndex, const FVector& Destination);

	/** Resets tracked patrol movement state when leaving Patrol or unpossessing. */
	void ResetPatrolNavigationState();

	/** Proximity and eligibility scan for player-controlled pawns and registered candidate targets. */
	void UpdatePlaceholderTargetAcquisition();

private:
	/** Cached pointer to the possessed pawn's behavior component. */
	UPROPERTY(Transient)
	TObjectPtr<USLAYCREWEnemyBehaviorComponent> CachedBehaviorComponent = nullptr;

	/** Optional candidate target actors (e.g., companions) considered alongside player controllers. */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> CandidateTargetActors;

	/** Target actor for which the current Chase MoveToActor command was issued. */
	TWeakObjectPtr<AActor> ActiveChaseTarget = nullptr;

	/** Waypoint index for which the current patrol MoveToLocation command was issued. */
	int32 ActivePatrolWaypointIndex = INDEX_NONE;

	/** World-space destination for which the current patrol MoveToLocation command was issued. */
	FVector ActivePatrolDestination = FVector::ZeroVector;

	/** True once a MoveToLocation request has been issued for the current (ActivePatrolWaypointIndex, ActivePatrolDestination). */
	bool bHasIssuedPatrolMoveForCurrentWaypoint = false;

	/** True while MoveToLocation is executing synchronously inside IssuePatrolMoveRequest. */
	bool bIsIssuingPatrolMoveRequest = false;

	/** True if the last patrol MoveToLocation transitioned PathFollowingComponent into Moving status. */
	bool bWasPatrolPathFollowingActive = false;

	/** True if an active patrol move failed or was interrupted and awaits a recovery retry. */
	bool bPatrolMoveNeedsRecovery = false;

	/** Remaining cooldown seconds before retrying an interrupted patrol movement request. */
	float PatrolMoveRetryCooldown = 0.0f;

	/** Diagnostic counter of patrol MoveToLocation invocations. */
	int32 PatrolMoveRequestCount = 0;
};

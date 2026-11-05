// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AI/SLAYCREWEnemyBehaviorTypes.h"
#include "SLAYCREWEnemyAIController.generated.h"

class USLAYCREWEnemyBehaviorComponent;

/**
 * Minimal AI Controller that drives navigation and focus based on a possessed pawn's
 * USLAYCREWEnemyBehaviorComponent (Idle, Patrol, Chase, AttackIntent).
 * Does not depend on specific player character classes, health, or damage systems.
 */
UCLASS()
class SLAYCREW_API ASLAYCREWEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASLAYCREWEnemyAIController();

	// ~Begin AController Interface
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;
	// ~End AController Interface

	/** Returns the behavior component cached from the currently possessed pawn. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	USLAYCREWEnemyBehaviorComponent* GetEnemyBehaviorComponent() const { return CachedBehaviorComponent; }

public:
	/**
	 * If true, performs simple placeholder distance checks against player-controlled pawns
	 * to populate the behavior component's target actor when none is currently assigned.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI")
	bool bAutoDetectPlayerTargets = true;

	/** Acceptance radius in cm passed to MoveToActor while in the Chase state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI", meta = (ClampMin = "10.0", Units = "cm"))
	float ChaseMoveAcceptanceRadius = 120.0f;

protected:
	/** Responds to state transitions emitted by the possessed pawn's behavior component. */
	UFUNCTION()
	void HandleBehaviorStateChanged(ESLAYCREWEnemyBehaviorState PreviousState, ESLAYCREWEnemyBehaviorState NewState);

	/** Responds to placeholder attack intent events emitted by the behavior component. */
	UFUNCTION()
	void HandleAttackIntent(AActor* TargetActor);

	/** Applies movement and focus commands matching the active behavior state. */
	void ApplyStateMovementCommand(ESLAYCREWEnemyBehaviorState State);

	/** Placeholder proximity scan for player-controlled pawns within detection range. */
	void UpdatePlaceholderTargetAcquisition();

private:
	/** Cached pointer to the possessed pawn's behavior component. */
	UPROPERTY(Transient)
	TObjectPtr<USLAYCREWEnemyBehaviorComponent> CachedBehaviorComponent = nullptr;
};

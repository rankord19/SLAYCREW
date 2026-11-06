// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AI/SLAYCREWEnemyBehaviorTypes.h"
#include "SLAYCREWEnemyCharacter.generated.h"

class USLAYCREWEnemyBehaviorComponent;

/**
 * Base enemy character class wired with USLAYCREWEnemyBehaviorComponent and ASLAYCREWEnemyAIController.
 * Provides modular state-driven movement speed adjustments and Blueprint hooks for Idle, Patrol,
 * Chase, and AttackIntent without coupling to health or damage systems.
 */
UCLASS()
class SLAYCREW_API ASLAYCREWEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASLAYCREWEnemyCharacter();

	/** Returns the enemy behavior state machine component. */
	UFUNCTION(BlueprintPure, Category = "SLAYCREW|AI")
	USLAYCREWEnemyBehaviorComponent* GetEnemyBehaviorComponent() const { return EnemyBehaviorComponent; }

public:
	/** Optional level actors whose locations seed the behavior component's patrol waypoints on BeginPlay. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "SLAYCREW|AI|Patrol")
	TArray<TObjectPtr<AActor>> PatrolWaypointActors;

	/** Character walk speed in cm/s when in Idle or Patrol states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float PatrolWalkSpeed = 250.0f;

	/** Character walk speed in cm/s when in Chase or AttackIntent states. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SLAYCREW|AI|Movement", meta = (ClampMin = "0.0", Units = "cm/s"))
	float ChaseWalkSpeed = 500.0f;

protected:
	// ~Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// ~End AActor Interface

	/** Internal callback when the behavior component transitions states. */
	UFUNCTION()
	void HandleBehaviorStateChanged(ESLAYCREWEnemyBehaviorState PreviousState, ESLAYCREWEnemyBehaviorState NewState);

	/** Internal callback when the behavior component signals placeholder attack intent. */
	UFUNCTION()
	void HandleAttackIntent(AActor* TargetActor);

	/** Blueprint hook invoked whenever the enemy transitions behavior states. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SLAYCREW|AI", meta = (DisplayName = "On Behavior State Changed"))
	void ReceiveBehaviorStateChanged(ESLAYCREWEnemyBehaviorState PreviousState, ESLAYCREWEnemyBehaviorState NewState);

	/** Blueprint hook invoked when the enemy triggers placeholder attack intent. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SLAYCREW|AI", meta = (DisplayName = "On Attack Intent"))
	void ReceiveAttackIntent(AActor* TargetActor);

	/** Updates CharacterMovement MaxWalkSpeed to match the active behavior state. */
	void UpdateMovementSpeedForState(ESLAYCREWEnemyBehaviorState State);

private:
	/** Modular enemy behavior component managing Idle, Patrol, Chase, and AttackIntent states. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USLAYCREWEnemyBehaviorComponent> EnemyBehaviorComponent;
};

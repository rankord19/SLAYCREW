// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/SLAYCREWEnemyAIController.h"
#include "AI/SLAYCREWEnemyBehaviorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "SLAYCREW.h"

ASLAYCREWEnemyAIController::ASLAYCREWEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ASLAYCREWEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (InPawn)
	{
		CachedBehaviorComponent = InPawn->FindComponentByClass<USLAYCREWEnemyBehaviorComponent>();
		if (CachedBehaviorComponent)
		{
			CachedBehaviorComponent->OnBehaviorStateChanged.AddDynamic(
				this,
				&ASLAYCREWEnemyAIController::HandleBehaviorStateChanged);
			CachedBehaviorComponent->OnAttackIntent.AddDynamic(
				this,
				&ASLAYCREWEnemyAIController::HandleAttackIntent);

			ApplyStateMovementCommand(CachedBehaviorComponent->GetCurrentState());
		}
	}
}

void ASLAYCREWEnemyAIController::OnUnPossess()
{
	if (CachedBehaviorComponent)
	{
		CachedBehaviorComponent->OnBehaviorStateChanged.RemoveDynamic(
			this,
			&ASLAYCREWEnemyAIController::HandleBehaviorStateChanged);
		CachedBehaviorComponent->OnAttackIntent.RemoveDynamic(
			this,
			&ASLAYCREWEnemyAIController::HandleAttackIntent);
		CachedBehaviorComponent = nullptr;
	}

	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);

	Super::OnUnPossess();
}

void ASLAYCREWEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority() || !CachedBehaviorComponent)
	{
		return;
	}

	if (bAutoDetectPlayerTargets && !IsValid(CachedBehaviorComponent->GetTargetActor()))
	{
		UpdatePlaceholderTargetAcquisition();
	}

	// Keep navigation command updated for Patrol when waypoints advance without changing state
	if (CachedBehaviorComponent->GetCurrentState() == ESLAYCREWEnemyBehaviorState::Patrol)
	{
		FVector CurrentWaypoint = FVector::ZeroVector;
		if (CachedBehaviorComponent->GetCurrentPatrolWaypoint(CurrentWaypoint))
		{
			MoveToLocation(
				CurrentWaypoint,
				FMath::Max(CachedBehaviorComponent->BehaviorConfig.PatrolAcceptanceRadius * 0.5f, 10.0f));
		}
	}
}

void ASLAYCREWEnemyAIController::HandleBehaviorStateChanged(
	ESLAYCREWEnemyBehaviorState PreviousState,
	ESLAYCREWEnemyBehaviorState NewState)
{
	ApplyStateMovementCommand(NewState);
}

void ASLAYCREWEnemyAIController::HandleAttackIntent(AActor* TargetActor)
{
	if (IsValid(TargetActor))
	{
		SetFocus(TargetActor, EAIFocusPriority::Gameplay);
	}
}

void ASLAYCREWEnemyAIController::ApplyStateMovementCommand(ESLAYCREWEnemyBehaviorState State)
{
	if (!HasAuthority() || !CachedBehaviorComponent)
	{
		return;
	}

	switch (State)
	{
	case ESLAYCREWEnemyBehaviorState::Idle:
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		break;

	case ESLAYCREWEnemyBehaviorState::Patrol:
	{
		ClearFocus(EAIFocusPriority::Gameplay);
		FVector PatrolDestination = FVector::ZeroVector;
		if (CachedBehaviorComponent->GetCurrentPatrolWaypoint(PatrolDestination))
		{
			MoveToLocation(
				PatrolDestination,
				FMath::Max(CachedBehaviorComponent->BehaviorConfig.PatrolAcceptanceRadius * 0.5f, 10.0f));
		}
		break;
	}

	case ESLAYCREWEnemyBehaviorState::Chase:
	{
		AActor* Target = CachedBehaviorComponent->GetTargetActor();
		if (IsValid(Target))
		{
			SetFocus(Target, EAIFocusPriority::Gameplay);
			MoveToActor(Target, ChaseMoveAcceptanceRadius);
		}
		else
		{
			StopMovement();
		}
		break;
	}

	case ESLAYCREWEnemyBehaviorState::AttackIntent:
	{
		StopMovement();
		AActor* Target = CachedBehaviorComponent->GetTargetActor();
		if (IsValid(Target))
		{
			SetFocus(Target, EAIFocusPriority::Gameplay);
		}
		break;
	}

	default:
		StopMovement();
		break;
	}
}

void ASLAYCREWEnemyAIController::UpdatePlaceholderTargetAcquisition()
{
	const APawn* ControlledPawn = GetPawn();
	const UWorld* World = GetWorld();
	if (!ControlledPawn || !World || !CachedBehaviorComponent)
	{
		return;
	}

	const FVector Origin = ControlledPawn->GetActorLocation();
	const float MaxDetectionRangeSq = FMath::Square(CachedBehaviorComponent->BehaviorConfig.ChaseDetectionRange);

	APawn* BestCandidate = nullptr;
	float ClosestDistSq = MaxDetectionRangeSq;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
		if (IsValid(PlayerPawn) && PlayerPawn != ControlledPawn)
		{
			const float DistSq = FVector::DistSquared(Origin, PlayerPawn->GetActorLocation());
			if (DistSq <= ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				BestCandidate = PlayerPawn;
			}
		}
	}

	if (BestCandidate)
	{
		CachedBehaviorComponent->SetTargetActor(BestCandidate);
	}
}

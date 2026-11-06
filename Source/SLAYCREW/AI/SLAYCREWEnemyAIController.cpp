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

	ResetPatrolNavigationState();
	ActiveChaseTarget = nullptr;

	if (InPawn)
	{
		CachedBehaviorComponent = InPawn->FindComponentByClass<USLAYCREWEnemyBehaviorComponent>();
		if (CachedBehaviorComponent)
		{
			CachedBehaviorComponent->OnBehaviorStateChanged.AddDynamic(
				this,
				&ASLAYCREWEnemyAIController::HandleBehaviorStateChanged);
			CachedBehaviorComponent->OnPatrolWaypointChanged.AddDynamic(
				this,
				&ASLAYCREWEnemyAIController::HandlePatrolWaypointChanged);
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
		CachedBehaviorComponent->OnPatrolWaypointChanged.RemoveDynamic(
			this,
			&ASLAYCREWEnemyAIController::HandlePatrolWaypointChanged);
		CachedBehaviorComponent->OnAttackIntent.RemoveDynamic(
			this,
			&ASLAYCREWEnemyAIController::HandleAttackIntent);
		CachedBehaviorComponent = nullptr;
	}

	ResetPatrolNavigationState();
	ActiveChaseTarget = nullptr;
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

	// Drop current target immediately if it died, became incapacitated, or otherwise lost combat eligibility
	if (CachedBehaviorComponent->GetTargetActor() != nullptr && !CachedBehaviorComponent->HasValidCombatTarget())
	{
		CachedBehaviorComponent->ClearTargetActor();
		ActiveChaseTarget = nullptr;
		CachedBehaviorComponent->EvaluateBehavior(0.0f);
	}

	if (bAutoDetectPlayerTargets && !CachedBehaviorComponent->HasValidCombatTarget())
	{
		UpdatePlaceholderTargetAcquisition();
	}

	const ESLAYCREWEnemyBehaviorState CurrentState = CachedBehaviorComponent->GetCurrentState();
	if (CurrentState == ESLAYCREWEnemyBehaviorState::Patrol)
	{
		UpdatePatrolNavigation(DeltaSeconds);
	}
	else if (CurrentState == ESLAYCREWEnemyBehaviorState::Chase)
	{
		if (CachedBehaviorComponent->HasValidCombatTarget())
		{
			AActor* Target = CachedBehaviorComponent->GetTargetActor();
			if (Target != ActiveChaseTarget.Get())
			{
				ActiveChaseTarget = Target;
				SetFocus(Target, EAIFocusPriority::Gameplay);
				MoveToActor(Target, ChaseMoveAcceptanceRadius);
			}
		}
		else if (ActiveChaseTarget.IsValid())
		{
			ActiveChaseTarget = nullptr;
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
		}
	}
}

void ASLAYCREWEnemyAIController::OnMoveCompleted(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	Super::OnMoveCompleted(RequestID, Result);

	if (!HasAuthority() || !CachedBehaviorComponent)
	{
		return;
	}

	if (CachedBehaviorComponent->GetCurrentState() == ESLAYCREWEnemyBehaviorState::Patrol)
	{
		bWasPatrolPathFollowingActive = false;
		if (!Result.IsSuccess())
		{
			const bool bMissingNavSystemSynchronousFail =
				bIsIssuingPatrolMoveRequest &&
				Result.Code == EPathFollowingResult::Invalid &&
				(!GetWorld() || GetWorld()->GetNavigationSystem() == nullptr);

			if (!bMissingNavSystemSynchronousFail)
			{
				bPatrolMoveNeedsRecovery = true;
				PatrolMoveRetryCooldown = FMath::Max(PatrolMoveRetryInterval, 0.05f);
			}
		}
	}
}

void ASLAYCREWEnemyAIController::RegisterCandidateTarget(AActor* CandidateActor)
{
	if (!IsValid(CandidateActor))
	{
		return;
	}

	for (const TWeakObjectPtr<AActor>& Existing : CandidateTargetActors)
	{
		if (Existing.Get() == CandidateActor)
		{
			return;
		}
	}

	CandidateTargetActors.Add(CandidateActor);
}

void ASLAYCREWEnemyAIController::UnregisterCandidateTarget(AActor* CandidateActor)
{
	CandidateTargetActors.RemoveAll([CandidateActor](const TWeakObjectPtr<AActor>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == CandidateActor;
	});
}

void ASLAYCREWEnemyAIController::NotifyPatrolMovementInterrupted()
{
	if (CachedBehaviorComponent && CachedBehaviorComponent->GetCurrentState() == ESLAYCREWEnemyBehaviorState::Patrol)
	{
		bWasPatrolPathFollowingActive = false;
		bPatrolMoveNeedsRecovery = true;
		PatrolMoveRetryCooldown = FMath::Max(PatrolMoveRetryInterval, 0.05f);
	}
}

void ASLAYCREWEnemyAIController::HandleBehaviorStateChanged(
	ESLAYCREWEnemyBehaviorState PreviousState,
	ESLAYCREWEnemyBehaviorState NewState)
{
	if (PreviousState == ESLAYCREWEnemyBehaviorState::Patrol && NewState != ESLAYCREWEnemyBehaviorState::Patrol)
	{
		ResetPatrolNavigationState();
	}
	if (PreviousState == ESLAYCREWEnemyBehaviorState::Chase && NewState != ESLAYCREWEnemyBehaviorState::Chase)
	{
		ActiveChaseTarget = nullptr;
	}

	ApplyStateMovementCommand(NewState);
}

void ASLAYCREWEnemyAIController::HandlePatrolWaypointChanged(int32 NewWaypointIndex, FVector NewWaypointLocation)
{
	if (!HasAuthority() || !CachedBehaviorComponent)
	{
		return;
	}

	if (CachedBehaviorComponent->GetCurrentState() == ESLAYCREWEnemyBehaviorState::Patrol)
	{
		if (!bHasIssuedPatrolMoveForCurrentWaypoint ||
			ActivePatrolWaypointIndex != NewWaypointIndex ||
			!ActivePatrolDestination.Equals(NewWaypointLocation, 1.0f))
		{
			IssuePatrolMoveRequest(NewWaypointIndex, NewWaypointLocation);
		}
	}
	else
	{
		ResetPatrolNavigationState();
	}
}

void ASLAYCREWEnemyAIController::HandleAttackIntent(AActor* TargetActor)
{
	if (CachedBehaviorComponent && CachedBehaviorComponent->IsTargetEligible(TargetActor))
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
		ResetPatrolNavigationState();
		ActiveChaseTarget = nullptr;
		StopMovement();
		ClearFocus(EAIFocusPriority::Gameplay);
		break;

	case ESLAYCREWEnemyBehaviorState::Patrol:
	{
		ActiveChaseTarget = nullptr;
		ClearFocus(EAIFocusPriority::Gameplay);
		ResetPatrolNavigationState();
		UpdatePatrolNavigation(0.0f);
		break;
	}

	case ESLAYCREWEnemyBehaviorState::Chase:
	{
		ResetPatrolNavigationState();
		if (CachedBehaviorComponent->HasValidCombatTarget())
		{
			AActor* Target = CachedBehaviorComponent->GetTargetActor();
			ActiveChaseTarget = Target;
			SetFocus(Target, EAIFocusPriority::Gameplay);
			MoveToActor(Target, ChaseMoveAcceptanceRadius);
		}
		else
		{
			ActiveChaseTarget = nullptr;
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
		}
		break;
	}

	case ESLAYCREWEnemyBehaviorState::AttackIntent:
	{
		ResetPatrolNavigationState();
		ActiveChaseTarget = nullptr;
		StopMovement();
		if (CachedBehaviorComponent->HasValidCombatTarget())
		{
			SetFocus(CachedBehaviorComponent->GetTargetActor(), EAIFocusPriority::Gameplay);
		}
		else
		{
			ClearFocus(EAIFocusPriority::Gameplay);
		}
		break;
	}

	default:
		ResetPatrolNavigationState();
		ActiveChaseTarget = nullptr;
		StopMovement();
		break;
	}
}

void ASLAYCREWEnemyAIController::UpdatePatrolNavigation(float DeltaSeconds)
{
	if (!HasAuthority() || !CachedBehaviorComponent ||
		CachedBehaviorComponent->GetCurrentState() != ESLAYCREWEnemyBehaviorState::Patrol)
	{
		return;
	}

	FVector CurrentWaypoint = FVector::ZeroVector;
	const int32 CurrentWaypointIndex = CachedBehaviorComponent->GetCurrentPatrolWaypointIndex();
	if (!CachedBehaviorComponent->GetCurrentPatrolWaypoint(CurrentWaypoint) || CurrentWaypointIndex == INDEX_NONE)
	{
		ResetPatrolNavigationState();
		return;
	}

	const bool bWaypointChanged =
		!bHasIssuedPatrolMoveForCurrentWaypoint ||
		(ActivePatrolWaypointIndex != CurrentWaypointIndex) ||
		!ActivePatrolDestination.Equals(CurrentWaypoint, 1.0f);

	if (bWaypointChanged)
	{
		IssuePatrolMoveRequest(CurrentWaypointIndex, CurrentWaypoint);
		return;
	}

	// Detect if an active path-following move unexpectedly dropped to Idle before reaching the waypoint
	if (bWasPatrolPathFollowingActive && !bPatrolMoveNeedsRecovery &&
		GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		const APawn* ControlledPawn = GetPawn();
		const float DistToWaypoint = ControlledPawn
			? FVector::Dist(ControlledPawn->GetActorLocation(), CurrentWaypoint)
			: 0.0f;
		if (DistToWaypoint > CachedBehaviorComponent->BehaviorConfig.PatrolAcceptanceRadius)
		{
			bWasPatrolPathFollowingActive = false;
			bPatrolMoveNeedsRecovery = true;
			PatrolMoveRetryCooldown = FMath::Max(PatrolMoveRetryInterval, 0.05f);
		}
	}

	if (bPatrolMoveNeedsRecovery)
	{
		PatrolMoveRetryCooldown -= DeltaSeconds;
		if (PatrolMoveRetryCooldown <= 0.0f)
		{
			IssuePatrolMoveRequest(CurrentWaypointIndex, CurrentWaypoint);
		}
	}
}

void ASLAYCREWEnemyAIController::IssuePatrolMoveRequest(int32 WaypointIndex, const FVector& Destination)
{
	if (!CachedBehaviorComponent)
	{
		return;
	}

	ActivePatrolWaypointIndex = WaypointIndex;
	ActivePatrolDestination = Destination;
	bHasIssuedPatrolMoveForCurrentWaypoint = true;
	bPatrolMoveNeedsRecovery = false;
	PatrolMoveRetryCooldown = 0.0f;
	++PatrolMoveRequestCount;

	const float AcceptanceRadius = FMath::Max(CachedBehaviorComponent->BehaviorConfig.PatrolAcceptanceRadius * 0.5f, 10.0f);
	TGuardValue<bool> IssuingMoveGuard(bIsIssuingPatrolMoveRequest, true);
	const EPathFollowingRequestResult::Type RequestResult = MoveToLocation(Destination, AcceptanceRadius);
	bWasPatrolPathFollowingActive =
		(RequestResult == EPathFollowingRequestResult::RequestSuccessful) &&
		(GetMoveStatus() != EPathFollowingStatus::Idle);
}

void ASLAYCREWEnemyAIController::ResetPatrolNavigationState()
{
	ActivePatrolWaypointIndex = INDEX_NONE;
	ActivePatrolDestination = FVector::ZeroVector;
	bHasIssuedPatrolMoveForCurrentWaypoint = false;
	bWasPatrolPathFollowingActive = false;
	bPatrolMoveNeedsRecovery = false;
	PatrolMoveRetryCooldown = 0.0f;
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

	AActor* BestCandidate = nullptr;
	float ClosestDistSq = MaxDetectionRangeSq;

	auto ConsiderCandidate = [&](AActor* Candidate)
	{
		if (!IsValid(Candidate) || Candidate == ControlledPawn)
		{
			return;
		}
		if (!CachedBehaviorComponent->IsTargetEligible(Candidate))
		{
			return;
		}

		const float DistSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistSq <= ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			BestCandidate = Candidate;
		}
	};

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
		ConsiderCandidate(PlayerPawn);
	}

	for (int32 Index = CandidateTargetActors.Num() - 1; Index >= 0; --Index)
	{
		AActor* Candidate = CandidateTargetActors[Index].Get();
		if (!IsValid(Candidate))
		{
			CandidateTargetActors.RemoveAtSwap(Index);
			continue;
		}
		ConsiderCandidate(Candidate);
	}

	if (BestCandidate)
	{
		CachedBehaviorComponent->SetTargetActor(BestCandidate);
		CachedBehaviorComponent->EvaluateBehavior(0.0f);
	}
}

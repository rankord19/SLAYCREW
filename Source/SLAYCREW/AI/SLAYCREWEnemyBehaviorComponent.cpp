// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/SLAYCREWEnemyBehaviorComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "SLAYCREW.h"

USLAYCREWEnemyBehaviorComponent::USLAYCREWEnemyBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void USLAYCREWEnemyBehaviorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (PatrolWaypoints.Num() > 0 && CurrentPatrolIndex == INDEX_NONE)
	{
		CurrentPatrolIndex = 0;
	}
}

void USLAYCREWEnemyBehaviorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (HasBehaviorAuthority())
	{
		EvaluateBehavior(DeltaTime);
	}
}

void USLAYCREWEnemyBehaviorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USLAYCREWEnemyBehaviorComponent, CurrentState);
	DOREPLIFETIME(USLAYCREWEnemyBehaviorComponent, CurrentTarget);
}

void USLAYCREWEnemyBehaviorComponent::EvaluateBehavior(float DeltaTime)
{
	if (!HasBehaviorAuthority() || DeltaTime < 0.0f)
	{
		return;
	}

	TimeInCurrentState += DeltaTime;

	const AActor* OwnerActor = GetOwner();
	const FVector OwnerLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

	const bool bHasValidTarget = IsValid(CurrentTarget);
	float DistanceToTarget = TNumericLimits<float>::Max();
	if (bHasValidTarget)
	{
		DistanceToTarget = FVector::Dist(OwnerLocation, CurrentTarget->GetActorLocation());
	}

	// If an active target moved beyond LoseTargetRange while chasing/engaging, drop the target.
	if (bHasValidTarget &&
		(CurrentState == ESLAYCREWEnemyBehaviorState::Chase || CurrentState == ESLAYCREWEnemyBehaviorState::AttackIntent) &&
		DistanceToTarget > BehaviorConfig.LoseTargetRange)
	{
		ClearTargetActor();
	}

	const bool bStillHasValidTarget = IsValid(CurrentTarget);
	const bool bHasPatrolPoints = HasActivePatrolWaypoints();

	const ESLAYCREWEnemyBehaviorState DesiredState = DetermineDesiredState(
		CurrentState,
		bStillHasValidTarget,
		DistanceToTarget,
		bHasPatrolPoints,
		TimeInCurrentState);

	if (DesiredState != CurrentState)
	{
		SetBehaviorState(DesiredState);
		return;
	}

	// Per-state ongoing updates when no immediate state transition occurred
	if (CurrentState == ESLAYCREWEnemyBehaviorState::Patrol)
	{
		FVector TargetWaypoint = FVector::ZeroVector;
		if (OwnerActor && GetCurrentPatrolWaypoint(TargetWaypoint))
		{
			const float DistanceToWaypoint = FVector::Dist(OwnerLocation, TargetWaypoint);
			if (DistanceToWaypoint <= BehaviorConfig.PatrolAcceptanceRadius)
			{
				const bool bHasNextWaypoint = AdvancePatrolWaypoint();
				if (!bHasNextWaypoint || BehaviorConfig.bPauseAtPatrolWaypoints)
				{
					SetBehaviorState(ESLAYCREWEnemyBehaviorState::Idle);
				}
			}
		}
	}
	else if (CurrentState == ESLAYCREWEnemyBehaviorState::AttackIntent)
	{
		AttackIntentTimer -= DeltaTime;
		if (AttackIntentTimer <= 0.0f)
		{
			TriggerAttackIntent();
			AttackIntentTimer = FMath::Max(BehaviorConfig.AttackIntentInterval, 0.1f);
		}
	}
}

ESLAYCREWEnemyBehaviorState USLAYCREWEnemyBehaviorComponent::DetermineDesiredState(
	ESLAYCREWEnemyBehaviorState InCurrentState,
	bool bHasValidTarget,
	float DistanceToTarget,
	bool bHasPatrolPoints,
	float InTimeInState) const
{
	switch (InCurrentState)
	{
	case ESLAYCREWEnemyBehaviorState::Idle:
		if (bHasValidTarget)
		{
			if (DistanceToTarget <= BehaviorConfig.AttackIntentRange)
			{
				return ESLAYCREWEnemyBehaviorState::AttackIntent;
			}
			if (DistanceToTarget <= BehaviorConfig.ChaseDetectionRange)
			{
				return ESLAYCREWEnemyBehaviorState::Chase;
			}
		}
		if (bHasPatrolPoints && InTimeInState >= BehaviorConfig.IdleDuration)
		{
			return ESLAYCREWEnemyBehaviorState::Patrol;
		}
		return ESLAYCREWEnemyBehaviorState::Idle;

	case ESLAYCREWEnemyBehaviorState::Patrol:
		if (bHasValidTarget)
		{
			if (DistanceToTarget <= BehaviorConfig.AttackIntentRange)
			{
				return ESLAYCREWEnemyBehaviorState::AttackIntent;
			}
			if (DistanceToTarget <= BehaviorConfig.ChaseDetectionRange)
			{
				return ESLAYCREWEnemyBehaviorState::Chase;
			}
		}
		if (!bHasPatrolPoints)
		{
			return ESLAYCREWEnemyBehaviorState::Idle;
		}
		return ESLAYCREWEnemyBehaviorState::Patrol;

	case ESLAYCREWEnemyBehaviorState::Chase:
		if (!bHasValidTarget || DistanceToTarget > BehaviorConfig.LoseTargetRange)
		{
			return ESLAYCREWEnemyBehaviorState::Idle;
		}
		if (DistanceToTarget <= BehaviorConfig.AttackIntentRange)
		{
			return ESLAYCREWEnemyBehaviorState::AttackIntent;
		}
		return ESLAYCREWEnemyBehaviorState::Chase;

	case ESLAYCREWEnemyBehaviorState::AttackIntent:
		if (!bHasValidTarget || DistanceToTarget > BehaviorConfig.LoseTargetRange)
		{
			return ESLAYCREWEnemyBehaviorState::Idle;
		}
		if (DistanceToTarget > BehaviorConfig.AttackIntentDisengageRange)
		{
			return ESLAYCREWEnemyBehaviorState::Chase;
		}
		return ESLAYCREWEnemyBehaviorState::AttackIntent;

	default:
		return ESLAYCREWEnemyBehaviorState::Idle;
	}
}

bool USLAYCREWEnemyBehaviorComponent::SetBehaviorState(ESLAYCREWEnemyBehaviorState NewState)
{
	if (!HasBehaviorAuthority() || CurrentState == NewState)
	{
		return false;
	}

	const ESLAYCREWEnemyBehaviorState PreviousState = CurrentState;
	CurrentState = NewState;
	OnEnterState(PreviousState, NewState);
	return true;
}

void USLAYCREWEnemyBehaviorComponent::SetTargetActor(AActor* NewTarget)
{
	if (!HasBehaviorAuthority())
	{
		return;
	}

	CurrentTarget = NewTarget;
}

void USLAYCREWEnemyBehaviorComponent::ClearTargetActor()
{
	if (!HasBehaviorAuthority())
	{
		return;
	}

	CurrentTarget = nullptr;
}

void USLAYCREWEnemyBehaviorComponent::SetPatrolWaypoints(const TArray<FVector>& InWaypoints)
{
	PatrolWaypoints = InWaypoints;
	CurrentPatrolIndex = (PatrolWaypoints.Num() > 0) ? 0 : INDEX_NONE;
}

void USLAYCREWEnemyBehaviorComponent::AddPatrolWaypoint(const FVector& Waypoint)
{
	PatrolWaypoints.Add(Waypoint);
	if (CurrentPatrolIndex == INDEX_NONE)
	{
		CurrentPatrolIndex = 0;
	}
}

bool USLAYCREWEnemyBehaviorComponent::GetCurrentPatrolWaypoint(FVector& OutWaypoint) const
{
	if (PatrolWaypoints.IsValidIndex(CurrentPatrolIndex))
	{
		OutWaypoint = PatrolWaypoints[CurrentPatrolIndex];
		return true;
	}
	return false;
}

bool USLAYCREWEnemyBehaviorComponent::AdvancePatrolWaypoint()
{
	if (PatrolWaypoints.Num() == 0)
	{
		CurrentPatrolIndex = INDEX_NONE;
		return false;
	}

	const int32 NextIndex = CurrentPatrolIndex + 1;
	if (PatrolWaypoints.IsValidIndex(NextIndex))
	{
		CurrentPatrolIndex = NextIndex;
		return true;
	}

	if (BehaviorConfig.bLoopPatrol)
	{
		CurrentPatrolIndex = 0;
		return true;
	}

	CurrentPatrolIndex = INDEX_NONE;
	return false;
}

bool USLAYCREWEnemyBehaviorComponent::HasActivePatrolWaypoints() const
{
	return PatrolWaypoints.IsValidIndex(CurrentPatrolIndex);
}

void USLAYCREWEnemyBehaviorComponent::TriggerAttackIntent()
{
	if (!HasBehaviorAuthority())
	{
		return;
	}

	UE_LOG(
		LogSLAYCREW,
		Verbose,
		TEXT("[%s] Enemy AttackIntent triggered against target [%s] (placeholder)."),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(CurrentTarget));

	OnAttackIntent.Broadcast(CurrentTarget);
}

bool USLAYCREWEnemyBehaviorComponent::HasBehaviorAuthority() const
{
	const AActor* OwnerActor = GetOwner();
	return (OwnerActor == nullptr) || OwnerActor->HasAuthority();
}

void USLAYCREWEnemyBehaviorComponent::OnRep_CurrentState(ESLAYCREWEnemyBehaviorState PreviousState)
{
	TimeInCurrentState = 0.0f;
	OnBehaviorStateChanged.Broadcast(PreviousState, CurrentState);
}

void USLAYCREWEnemyBehaviorComponent::OnEnterState(
	ESLAYCREWEnemyBehaviorState PreviousState,
	ESLAYCREWEnemyBehaviorState NewState)
{
	TimeInCurrentState = 0.0f;

	if (NewState == ESLAYCREWEnemyBehaviorState::Patrol && CurrentPatrolIndex == INDEX_NONE && PatrolWaypoints.Num() > 0)
	{
		CurrentPatrolIndex = 0;
	}

	UE_LOG(
		LogSLAYCREW,
		Verbose,
		TEXT("[%s] Enemy behavior state changed: %d -> %d"),
		*GetNameSafe(GetOwner()),
		static_cast<int32>(PreviousState),
		static_cast<int32>(NewState));

	OnBehaviorStateChanged.Broadcast(PreviousState, NewState);

	if (NewState == ESLAYCREWEnemyBehaviorState::AttackIntent)
	{
		// Signal initial attack intent immediately upon entering AttackIntent, then start the interval timer.
		AttackIntentTimer = FMath::Max(BehaviorConfig.AttackIntentInterval, 0.1f);
		TriggerAttackIntent();
	}
}

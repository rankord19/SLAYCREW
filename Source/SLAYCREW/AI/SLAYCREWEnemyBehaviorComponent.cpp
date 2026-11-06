// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/SLAYCREWEnemyBehaviorComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"
#include "SLAYCREW.h"

namespace
{
	bool TryCallZeroArgBoolFunction(const UObject* Object, FName FunctionName, bool& OutValue)
	{
		if (!IsValid(Object))
		{
			return false;
		}

		UFunction* Func = Object->FindFunction(FunctionName);
		if (!Func || Func->NumParms != 1)
		{
			return false;
		}

		const FBoolProperty* ReturnProp = CastField<FBoolProperty>(Func->GetReturnProperty());
		if (!ReturnProp)
		{
			return false;
		}

		TArray<uint8, TInlineAllocator<16>> ParmsBuffer;
		ParmsBuffer.AddZeroed(Func->ParmsSize);
		const_cast<UObject*>(Object)->ProcessEvent(Func, ParmsBuffer.GetData());
		OutValue = ReturnProp->GetPropertyValue_InContainer(ParmsBuffer.GetData());
		return true;
	}

	bool TryCallZeroArgFloatFunction(const UObject* Object, FName FunctionName, float& OutValue)
	{
		if (!IsValid(Object))
		{
			return false;
		}

		UFunction* Func = Object->FindFunction(FunctionName);
		if (!Func || Func->NumParms != 1)
		{
			return false;
		}

		const FProperty* ReturnProp = Func->GetReturnProperty();
		const FFloatProperty* FloatProp = CastField<FFloatProperty>(ReturnProp);
		const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(ReturnProp);
		if (!FloatProp && !DoubleProp)
		{
			return false;
		}

		TArray<uint8, TInlineAllocator<16>> ParmsBuffer;
		ParmsBuffer.AddZeroed(Func->ParmsSize);
		const_cast<UObject*>(Object)->ProcessEvent(Func, ParmsBuffer.GetData());
		if (FloatProp)
		{
			OutValue = FloatProp->GetPropertyValue_InContainer(ParmsBuffer.GetData());
		}
		else
		{
			OutValue = static_cast<float>(DoubleProp->GetPropertyValue_InContainer(ParmsBuffer.GetData()));
		}
		return true;
	}

	bool IsObjectCombatCapableByReflection(const UObject* Object)
	{
		if (!IsValid(Object))
		{
			return false;
		}

		if (Object->GetClass()->ImplementsInterface(USLAYCREWCombatTargetInterface::StaticClass()))
		{
			if (!ISLAYCREWCombatTargetInterface::Execute_IsTargetCombatCapable(Object))
			{
				return false;
			}
		}

		bool bFlag = false;
		// Disqualifying true flags (e.g., USlayHealthComponent::IsDead, USlayHealthComponent::IsDepleted, IsIncapacitated)
		static const FName DisqualifyingTrueFuncs[] = {
			FName(TEXT("IsDead")),
			FName(TEXT("IsIncapacitated")),
			FName(TEXT("IsDepleted"))
		};
		for (const FName& FuncName : DisqualifyingTrueFuncs)
		{
			if (TryCallZeroArgBoolFunction(Object, FuncName, bFlag) && bFlag)
			{
				return false;
			}
		}

		// Required true capability flags when present (e.g., USlayInjuryComponent::CanAct, ASlayFighterCharacter::CanPerformActions)
		static const FName RequiredTrueFuncs[] = {
			FName(TEXT("IsAlive")),
			FName(TEXT("CanAct")),
			FName(TEXT("CanPerformActions")),
			FName(TEXT("CanBeTargeted"))
		};
		for (const FName& FuncName : RequiredTrueFuncs)
		{
			if (TryCallZeroArgBoolFunction(Object, FuncName, bFlag) && !bFlag)
			{
				return false;
			}
		}

		float CurrentHealth = 1.0f;
		if (TryCallZeroArgFloatFunction(Object, FName(TEXT("GetCurrentHealth")), CurrentHealth) && CurrentHealth <= 0.0f)
		{
			return false;
		}

		return true;
	}

	bool HasDisqualifyingTags(const TArray<FName>& Tags)
	{
		static const FName DisqualifyingTags[] = {
			FName(TEXT("Dead")),
			FName(TEXT("Incapacitated")),
			FName(TEXT("Downed")),
			FName(TEXT("Untargetable"))
		};
		for (const FName& DisqualifyingTag : DisqualifyingTags)
		{
			if (Tags.Contains(DisqualifyingTag))
			{
				return true;
			}
		}
		return false;
	}
}

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

bool USLAYCREWEnemyBehaviorComponent::IsTargetEligible(const AActor* CandidateTarget) const
{
	if (!IsValid(CandidateTarget) || CandidateTarget->IsActorBeingDestroyed() || CandidateTarget->GetTearOff())
	{
		return false;
	}

	if (CandidateTarget == GetOwner())
	{
		return false;
	}

	if (CustomTargetEligibilityPredicate.IsBound())
	{
		if (!CustomTargetEligibilityPredicate.Execute(CandidateTarget))
		{
			return false;
		}
	}

	if (HasDisqualifyingTags(CandidateTarget->Tags))
	{
		return false;
	}

	if (!IsObjectCombatCapableByReflection(CandidateTarget))
	{
		return false;
	}

	TInlineComponentArray<const UActorComponent*> Components(CandidateTarget);
	for (const UActorComponent* Comp : Components)
	{
		if (!IsValid(Comp))
		{
			continue;
		}

		if (HasDisqualifyingTags(Comp->ComponentTags))
		{
			return false;
		}

		if (!IsObjectCombatCapableByReflection(Comp))
		{
			return false;
		}
	}

	return true;
}

bool USLAYCREWEnemyBehaviorComponent::HasValidCombatTarget() const
{
	return IsTargetEligible(CurrentTarget);
}

void USLAYCREWEnemyBehaviorComponent::EvaluateBehavior(float DeltaTime)
{
	if (!HasBehaviorAuthority() || DeltaTime < 0.0f)
	{
		return;
	}

	TimeInCurrentState += DeltaTime;

	// Immediately clear CurrentTarget if it died, became incapacitated, or otherwise lost combat eligibility
	if (CurrentTarget != nullptr && !IsTargetEligible(CurrentTarget))
	{
		ClearTargetActor();
	}

	const AActor* OwnerActor = GetOwner();
	const FVector OwnerLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

	const bool bHasValidTarget = HasValidCombatTarget();
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

	const bool bStillHasValidTarget = HasValidCombatTarget();
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
				if (BehaviorConfig.bPauseAtPatrolWaypoints)
				{
					SetBehaviorState(ESLAYCREWEnemyBehaviorState::Idle);
					AdvancePatrolWaypoint();
				}
				else if (!AdvancePatrolWaypoint())
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

	if (NewTarget != nullptr && !IsTargetEligible(NewTarget))
	{
		CurrentTarget = nullptr;
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
	if (PatrolWaypoints.IsValidIndex(CurrentPatrolIndex))
	{
		OnPatrolWaypointChanged.Broadcast(CurrentPatrolIndex, PatrolWaypoints[CurrentPatrolIndex]);
	}
}

void USLAYCREWEnemyBehaviorComponent::AddPatrolWaypoint(const FVector& Waypoint)
{
	PatrolWaypoints.Add(Waypoint);
	if (CurrentPatrolIndex == INDEX_NONE)
	{
		CurrentPatrolIndex = 0;
		OnPatrolWaypointChanged.Broadcast(CurrentPatrolIndex, PatrolWaypoints[CurrentPatrolIndex]);
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
		OnPatrolWaypointChanged.Broadcast(CurrentPatrolIndex, PatrolWaypoints[CurrentPatrolIndex]);
		return true;
	}

	if (BehaviorConfig.bLoopPatrol)
	{
		CurrentPatrolIndex = 0;
		OnPatrolWaypointChanged.Broadcast(CurrentPatrolIndex, PatrolWaypoints[CurrentPatrolIndex]);
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
	if (!HasBehaviorAuthority() || !HasValidCombatTarget())
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

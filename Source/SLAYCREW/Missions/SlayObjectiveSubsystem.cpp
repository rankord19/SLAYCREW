#include "Missions/SlayObjectiveSubsystem.h"

#include "Engine/World.h"
#include "SLAYCREW.h"

void USlayObjectiveSubsystem::Deinitialize()
{
	Objectives.Reset();
	OnObjectiveStateChanged.Clear();
	OnObjectiveProgressChanged.Clear();
	OnObjectiveStateChangedNative.Clear();
	OnObjectiveProgressChangedNative.Clear();
	Super::Deinitialize();
}

bool USlayObjectiveSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool USlayObjectiveSubsystem::HasAuthority() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() != NM_Client;
}

FSlayObjectiveStatus* USlayObjectiveSubsystem::FindStatus(FName ObjectiveId)
{
	return Objectives.FindByPredicate([ObjectiveId](const FSlayObjectiveStatus& Status)
	{
		return Status.Definition.ObjectiveId == ObjectiveId;
	});
}

const FSlayObjectiveStatus* USlayObjectiveSubsystem::FindStatus(FName ObjectiveId) const
{
	return const_cast<USlayObjectiveSubsystem*>(this)->FindStatus(ObjectiveId);
}

bool USlayObjectiveSubsystem::RegisterObjective(const FSlayObjectiveDefinition& Definition)
{
	if (!HasAuthority())
	{
		return false;
	}
	if (Definition.ObjectiveId.IsNone())
	{
		UE_LOG(LogSLAYCREW, Warning, TEXT("RegisterObjective: ObjectiveId must not be None."));
		return false;
	}
	if (FindStatus(Definition.ObjectiveId))
	{
		UE_LOG(LogSLAYCREW, Warning, TEXT("RegisterObjective: '%s' is already registered."),
			*Definition.ObjectiveId.ToString());
		return false;
	}

	FSlayObjectiveStatus& Status = Objectives.AddDefaulted_GetRef();
	Status.Definition = Definition;
	Status.Definition.TargetProgress = FMath::Max(1, Definition.TargetProgress);

	if (Definition.bActivateOnRegister)
	{
		ActivateObjective(Definition.ObjectiveId);
	}
	return true;
}

bool USlayObjectiveSubsystem::ActivateObjective(FName ObjectiveId)
{
	return TryTransition(ObjectiveId, ESlayObjectiveState::Inactive, ESlayObjectiveState::Active);
}

bool USlayObjectiveSubsystem::AddObjectiveProgress(FName ObjectiveId, int32 Delta)
{
	if (!HasAuthority() || Delta <= 0)
	{
		return false;
	}
	FSlayObjectiveStatus* Status = FindStatus(ObjectiveId);
	if (!Status || Status->State != ESlayObjectiveState::Active)
	{
		return false;
	}

	const int32 Target = Status->Definition.TargetProgress;
	const int32 NewProgress = static_cast<int32>(FMath::Min<int64>(
		static_cast<int64>(Status->CurrentProgress) + Delta, Target));
	if (NewProgress == Status->CurrentProgress)
	{
		return false;
	}
	Status->CurrentProgress = NewProgress;

	// Listeners may re-enter and mutate the array, so do not touch Status after broadcasting.
	BroadcastProgress(ObjectiveId, NewProgress, Target);
	if (NewProgress >= Target)
	{
		CompleteObjective(ObjectiveId);
	}
	return true;
}

bool USlayObjectiveSubsystem::CompleteObjective(FName ObjectiveId)
{
	if (!HasAuthority())
	{
		return false;
	}
	FSlayObjectiveStatus* Status = FindStatus(ObjectiveId);
	if (!Status || Status->State != ESlayObjectiveState::Active)
	{
		return false;
	}
	const int32 Target = Status->Definition.TargetProgress;
	if (Status->CurrentProgress != Target)
	{
		Status->CurrentProgress = Target;
		BroadcastProgress(ObjectiveId, Target, Target);
	}
	return TryTransition(ObjectiveId, ESlayObjectiveState::Active, ESlayObjectiveState::Completed);
}

bool USlayObjectiveSubsystem::FailObjective(FName ObjectiveId)
{
	return TryTransition(ObjectiveId, ESlayObjectiveState::Active, ESlayObjectiveState::Failed);
}

bool USlayObjectiveSubsystem::GetObjectiveStatus(FName ObjectiveId, FSlayObjectiveStatus& OutStatus) const
{
	if (const FSlayObjectiveStatus* Status = FindStatus(ObjectiveId))
	{
		OutStatus = *Status;
		return true;
	}
	return false;
}

ESlayObjectiveState USlayObjectiveSubsystem::GetObjectiveState(FName ObjectiveId) const
{
	const FSlayObjectiveStatus* Status = FindStatus(ObjectiveId);
	return Status ? Status->State : ESlayObjectiveState::Inactive;
}

TArray<FName> USlayObjectiveSubsystem::GetObjectiveIdsInState(ESlayObjectiveState State) const
{
	TArray<FName> Result;
	for (const FSlayObjectiveStatus& Status : Objectives)
	{
		if (Status.State == State)
		{
			Result.Add(Status.Definition.ObjectiveId);
		}
	}
	return Result;
}

bool USlayObjectiveSubsystem::TryTransition(FName ObjectiveId, ESlayObjectiveState RequiredState,
	ESlayObjectiveState NewState)
{
	if (!HasAuthority())
	{
		return false;
	}
	FSlayObjectiveStatus* Status = FindStatus(ObjectiveId);
	if (!Status || Status->State != RequiredState)
	{
		return false;
	}
	Status->State = NewState;
	OnObjectiveStateChangedNative.Broadcast(ObjectiveId, RequiredState, NewState);
	OnObjectiveStateChanged.Broadcast(ObjectiveId, RequiredState, NewState);
	return true;
}

void USlayObjectiveSubsystem::BroadcastProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress)
{
	OnObjectiveProgressChangedNative.Broadcast(ObjectiveId, CurrentProgress, TargetProgress);
	OnObjectiveProgressChanged.Broadcast(ObjectiveId, CurrentProgress, TargetProgress);
}

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Missions/SlayObjectiveTypes.h"
#include "SlayObjectiveSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSlayObjectiveStateChanged,
	FName, ObjectiveId, ESlayObjectiveState, PreviousState, ESlayObjectiveState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSlayObjectiveProgressChanged,
	FName, ObjectiveId, int32, CurrentProgress, int32, TargetProgress);

DECLARE_MULTICAST_DELEGATE_ThreeParams(FSlayObjectiveStateChangedNative,
	FName /*ObjectiveId*/, ESlayObjectiveState /*PreviousState*/, ESlayObjectiveState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FSlayObjectiveProgressChangedNative,
	FName /*ObjectiveId*/, int32 /*CurrentProgress*/, int32 /*TargetProgress*/);

/**
 * Per-world registry of mission objectives. Exists automatically in game and PIE worlds,
 * so no map, GameMode or player Blueprint changes are needed to use it.
 *
 * Authority: mutating calls are ignored on network clients (NM_Client). State is NOT
 * replicated yet; clients currently see no objectives. See Docs/MissionObjectives.md.
 *
 * Transitions: Inactive -> Active -> Completed | Failed. Terminal states are final.
 */
UCLASS()
class SLAYCREW_API USlayObjectiveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Adds a new objective in the Inactive state. Fails on None or duplicate IDs. */
	UFUNCTION(BlueprintCallable, Category="Objectives")
	bool RegisterObjective(const FSlayObjectiveDefinition& Definition);

	/** Inactive -> Active. */
	UFUNCTION(BlueprintCallable, Category="Objectives")
	bool ActivateObjective(FName ObjectiveId);

	/** Adds positive progress to an Active objective, clamped to target. Completes on reaching target. */
	UFUNCTION(BlueprintCallable, Category="Objectives")
	bool AddObjectiveProgress(FName ObjectiveId, int32 Delta = 1);

	/** Active -> Completed. Progress is filled to target. */
	UFUNCTION(BlueprintCallable, Category="Objectives")
	bool CompleteObjective(FName ObjectiveId);

	/** Active -> Failed. */
	UFUNCTION(BlueprintCallable, Category="Objectives")
	bool FailObjective(FName ObjectiveId);

	UFUNCTION(BlueprintPure, Category="Objectives")
	bool HasObjective(FName ObjectiveId) const { return FindStatus(ObjectiveId) != nullptr; }

	/** Returns false if the objective is not registered. */
	UFUNCTION(BlueprintPure, Category="Objectives")
	bool GetObjectiveStatus(FName ObjectiveId, FSlayObjectiveStatus& OutStatus) const;

	/** Returns Inactive for unknown IDs; use HasObjective to disambiguate. */
	UFUNCTION(BlueprintPure, Category="Objectives")
	ESlayObjectiveState GetObjectiveState(FName ObjectiveId) const;

	UFUNCTION(BlueprintPure, Category="Objectives")
	TArray<FName> GetObjectiveIdsInState(ESlayObjectiveState State) const;

	/** In registration order. */
	UFUNCTION(BlueprintPure, Category="Objectives")
	TArray<FSlayObjectiveStatus> GetAllObjectives() const { return Objectives; }

	UPROPERTY(BlueprintAssignable, Category="Objectives")
	FSlayObjectiveStateChanged OnObjectiveStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Objectives")
	FSlayObjectiveProgressChanged OnObjectiveProgressChanged;

	/** Native mirrors of the events above, for C++ listeners that are not UObjects. */
	FSlayObjectiveStateChangedNative OnObjectiveStateChangedNative;
	FSlayObjectiveProgressChangedNative OnObjectiveProgressChangedNative;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TArray<FSlayObjectiveStatus> Objectives;

	bool HasAuthority() const;
	FSlayObjectiveStatus* FindStatus(FName ObjectiveId);
	const FSlayObjectiveStatus* FindStatus(FName ObjectiveId) const;

	/** Moves an objective from RequiredState to NewState and notifies. */
	bool TryTransition(FName ObjectiveId, ESlayObjectiveState RequiredState, ESlayObjectiveState NewState);
	void BroadcastProgress(FName ObjectiveId, int32 CurrentProgress, int32 TargetProgress);
};

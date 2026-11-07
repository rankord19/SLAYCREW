#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SlayNarrativeSubsystem.generated.h"

class USlayNarrativeState;
class USlayRelationshipProfileAsset;

/**
 * Game-instance owner of the narrative/relationship state. Lives for the whole session, so
 * state persists across chapter maps without any map, GameMode or character Blueprint changes.
 *
 * Access: C++ GetGameInstance()->GetSubsystem<USlayNarrativeSubsystem>()->GetNarrativeState(),
 * or the "Slay Narrative Subsystem" getter node in Blueprint.
 */
UCLASS()
class SLAYCREW_API USlayNarrativeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category="Narrative")
	USlayNarrativeState* GetNarrativeState() const { return State; }

	/** Registers every valid profile in the asset. Returns the number registered. */
	UFUNCTION(BlueprintCallable, Category="Narrative")
	int32 RegisterProfileAsset(const USlayRelationshipProfileAsset* Asset);

	/** Standalone slot save until a project-wide SaveGame exists. */
	UFUNCTION(BlueprintCallable, Category="Narrative|Save")
	bool SaveToSlot(const FString& SlotName, int32 UserIndex);

	UFUNCTION(BlueprintCallable, Category="Narrative|Save")
	bool LoadFromSlot(const FString& SlotName, int32 UserIndex, TArray<FString>& OutMessages);

private:
	UPROPERTY()
	TObjectPtr<USlayNarrativeState> State;
};

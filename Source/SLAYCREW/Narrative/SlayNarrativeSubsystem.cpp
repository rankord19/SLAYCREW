#include "Narrative/SlayNarrativeSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Narrative/SlayNarrativeSaveData.h"
#include "Narrative/SlayNarrativeState.h"
#include "Narrative/SlayRelationshipProfileAsset.h"

void USlayNarrativeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = NewObject<USlayNarrativeState>(this);
}

int32 USlayNarrativeSubsystem::RegisterProfileAsset(const USlayRelationshipProfileAsset* Asset)
{
	if (!Asset || !State)
	{
		return 0;
	}
	int32 Registered = 0;
	for (const FSlayRelationshipProfile& Profile : Asset->Profiles)
	{
		Registered += State->RegisterProfile(Profile) ? 1 : 0;
	}
	return Registered;
}

bool USlayNarrativeSubsystem::SaveToSlot(const FString& SlotName, int32 UserIndex)
{
	if (!State || SlotName.IsEmpty())
	{
		return false;
	}
	USlayNarrativeSaveGame* SaveGame = NewObject<USlayNarrativeSaveGame>();
	SaveGame->Narrative = State->ExportSaveData();
	return UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
}

bool USlayNarrativeSubsystem::LoadFromSlot(const FString& SlotName, int32 UserIndex, TArray<FString>& OutMessages)
{
	if (!State || SlotName.IsEmpty())
	{
		OutMessages.Add(TEXT("Invalid slot name."));
		return false;
	}
	const USlayNarrativeSaveGame* SaveGame = Cast<USlayNarrativeSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (!SaveGame)
	{
		OutMessages.Add(FString::Printf(TEXT("No narrative save in slot '%s'."), *SlotName));
		return false;
	}
	return State->ImportSaveData(SaveGame->Narrative, OutMessages);
}

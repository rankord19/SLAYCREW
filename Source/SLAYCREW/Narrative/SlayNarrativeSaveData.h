#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Narrative/SlayRelationshipTypes.h"
#include "SlayNarrativeSaveData.generated.h"

USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipSaveEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	FName CharacterId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	FName OtherCharacterId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	FSlayRelationshipValues Values;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	ESlayRomanceState RomanceState = ESlayRomanceState::None;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	TArray<FName> Flags;
};

USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayDecisionRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	FName DecisionId;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	FName OptionId;
};

/**
 * Compact, versioned snapshot of relationship and narrative state. Contains no configuration
 * (profiles are content), so new companions, events and flags can be added in later updates.
 * Embed it in the game's main USaveGame, or use USlayNarrativeSaveGame standalone.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayNarrativeSaveData
{
	GENERATED_BODY()

	/** Bump when the layout changes and add a migration step in USlayNarrativeState::ImportSaveData. */
	static constexpr int32 CurrentVersion = 1;

	/** 0 means "never written"; import rejects it. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	int32 Version = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	int32 Chapter = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	TArray<FSlayRelationshipSaveEntry> Relationships;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	TArray<FName> Flags;

	/** In the order decisions were first made. */
	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	TArray<FSlayDecisionRecord> Decisions;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category="Save")
	TArray<FName> SeenEvents;
};

/** Standalone save container until a project-wide SaveGame exists. */
UCLASS(BlueprintType)
class SLAYCREW_API USlayNarrativeSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, SaveGame, Category="Save")
	FSlayNarrativeSaveData Narrative;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Narrative/SlayNarrativeTypes.h"
#include "SlayNarrativeChoice.generated.h"

/** Adds Delta (scaled by the target profile's Gain/Loss percent) to one dimension. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipChange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName OtherCharacterId = FName(TEXT("Protagonist"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	ESlayRelationshipDimension Dimension = ESlayRelationshipDimension::Trust;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	int32 Delta = 0;
};

USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipFlagChange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName OtherCharacterId = FName(TEXT("Protagonist"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName Flag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	bool bSet = true;
};

/** Requests a romance transition; it must be allowed by the relationship profile's rules. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRomanceTransitionRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName OtherCharacterId = FName(TEXT("Protagonist"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	ESlayRomanceState NewState = ESlayRomanceState::None;
};

/**
 * One selectable option of a story decision and all its consequences. Usable as a DataTable row.
 * Applying is all-or-nothing: if any requirement or romance request is invalid, nothing changes.
 * Effects run in field order: decision record, relationship changes, relationship flags,
 * FlagsToSet, FlagsToClear, romance transitions.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayNarrativeChoice : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName DecisionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FName OptionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	FSlayNarrativeConditionSet Requirements;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	TArray<FSlayRelationshipChange> RelationshipChanges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	TArray<FSlayRelationshipFlagChange> RelationshipFlagChanges;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	TArray<FName> FlagsToSet;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	TArray<FName> FlagsToClear;

	/** Validated against the state before this choice's other effects. One request per relationship. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	TArray<FSlayRomanceTransitionRequest> RomanceTransitions;

	/** When false, the choice is unavailable once its DecisionId has been recorded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Choice")
	bool bAllowRepeat = false;
};

/** A conditional story event or conversation. Usable as a DataTable row. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayNarrativeEventDefinition : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event")
	FName EventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event")
	FSlayNarrativeConditionSet Conditions;

	/** When false, the event is unavailable once marked as seen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Event")
	bool bRepeatable = false;
};

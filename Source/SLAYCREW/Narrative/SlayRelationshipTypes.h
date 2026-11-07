#pragma once

#include "CoreMinimal.h"
#include "Narrative/SlayNarrativeTypes.h"
#include "SlayRelationshipTypes.generated.h"

/** Current value of every relationship dimension. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipValues
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Relationship")
	int32 Trust = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Relationship")
	int32 Friendship = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Relationship")
	int32 Affection = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Relationship")
	int32 Tension = 0;

	int32 Get(ESlayRelationshipDimension Dimension) const
	{
		switch (Dimension)
		{
		case ESlayRelationshipDimension::Trust:      return Trust;
		case ESlayRelationshipDimension::Friendship: return Friendship;
		case ESlayRelationshipDimension::Affection:  return Affection;
		case ESlayRelationshipDimension::Tension:    return Tension;
		}
		return 0;
	}

	void Set(ESlayRelationshipDimension Dimension, int32 NewValue)
	{
		switch (Dimension)
		{
		case ESlayRelationshipDimension::Trust:      Trust = NewValue; break;
		case ESlayRelationshipDimension::Friendship: Friendship = NewValue; break;
		case ESlayRelationshipDimension::Affection:  Affection = NewValue; break;
		case ESlayRelationshipDimension::Tension:    Tension = NewValue; break;
		}
	}
};

/** Per-character configuration of one dimension. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayDimensionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	int32 Min = -100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	int32 Max = 100;

	/** Starting value; characters who already know each other can start anywhere in range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	int32 Initial = 0;

	/** Scales positive changes (100 = unchanged). Lets characters respond differently to the same choice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship", meta=(ClampMin="0"))
	int32 GainPercent = 100;

	/** Scales negative changes (100 = unchanged). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship", meta=(ClampMin="0"))
	int32 LossPercent = 100;
};

USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipDimensions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FSlayDimensionConfig Trust;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FSlayDimensionConfig Friendship;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FSlayDimensionConfig Affection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FSlayDimensionConfig Tension;

	const FSlayDimensionConfig& Get(ESlayRelationshipDimension Dimension) const
	{
		switch (Dimension)
		{
		case ESlayRelationshipDimension::Friendship: return Friendship;
		case ESlayRelationshipDimension::Affection:  return Affection;
		case ESlayRelationshipDimension::Tension:    return Tension;
		default:                                     return Trust;
		}
	}
};

/**
 * Fires OnRelationshipThresholdCrossed whenever the comparison result changes.
 * When it becomes true and FlagOnReached is set, that relationship flag is set persistently.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipThreshold
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FName ThresholdId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	ESlayRelationshipDimension Dimension = ESlayRelationshipDimension::Trust;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	ESlayComparison Comparison = ESlayComparison::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	int32 Value = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FName FlagOnReached;
};

/**
 * An allowed romance transition for one relationship. Transitions not listed are not possible.
 * Rules entering Interested or Committed must include a story requirement (flag, decision,
 * seen event or custom condition); numeric values alone are rejected by validation.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRomanceTransitionRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Romance")
	ESlayRomanceState From = ESlayRomanceState::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Romance")
	ESlayRomanceState To = ESlayRomanceState::Interested;

	/** Character-specific prerequisites, evaluated against the state before the transition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Romance")
	FSlayNarrativeConditionSet Requirements;
};

/**
 * Static configuration of the relationship between two characters. Companion routes use
 * OtherCharacterId = Protagonist; companion-to-companion relationships use another companion ID.
 * The pair is unordered for lookups.
 */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FName OtherCharacterId = FName(TEXT("Protagonist"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	FSlayRelationshipDimensions Dimensions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	TArray<FSlayRelationshipThreshold> Thresholds;

	/** Relationship flags present when the relationship is first created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	TArray<FName> InitialFlags;

	/** Boundary: when false no romance transition is possible for this pair, whatever the values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Romance")
	bool bRomanceRouteAvailable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Romance")
	TArray<FSlayRomanceTransitionRule> RomanceTransitions;

	/** Free-form designer metadata; never read by the framework. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Relationship")
	TMap<FName, FString> Metadata;

	bool Matches(FName A, FName B) const
	{
		return (CharacterId == A && OtherCharacterId == B) || (CharacterId == B && OtherCharacterId == A);
	}
};

/** Runtime state of one relationship. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayRelationshipRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Relationship")
	FName CharacterId;

	UPROPERTY(BlueprintReadOnly, Category="Relationship")
	FName OtherCharacterId;

	UPROPERTY(BlueprintReadOnly, Category="Relationship")
	FSlayRelationshipValues Values;

	UPROPERTY(BlueprintReadOnly, Category="Relationship")
	ESlayRomanceState RomanceState = ESlayRomanceState::None;

	/** Persistent relationship-scoped flags (e.g. personal events unlocked). */
	UPROPERTY(BlueprintReadOnly, Category="Relationship")
	TSet<FName> Flags;

	bool Matches(FName A, FName B) const
	{
		return (CharacterId == A && OtherCharacterId == B) || (CharacterId == B && OtherCharacterId == A);
	}
};

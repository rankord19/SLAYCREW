#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SlayNarrativeTypes.generated.h"

class USlayNarrativeState;

/**
 * Independent relationship axes. They are tracked and modified separately and are
 * never collapsed into a single affinity score.
 */
UENUM(BlueprintType)
enum class ESlayRelationshipDimension : uint8
{
	Trust,
	/** Friendship / emotional closeness. */
	Friendship,
	/** Romantic attraction or affection. Independent from Friendship. */
	Affection,
	/** Tension / conflict. */
	Tension
};

/**
 * Romance route state of a relationship. Which transitions exist, and what they require,
 * is configured per relationship profile, so routes do not have to share one progression.
 */
UENUM(BlueprintType)
enum class ESlayRomanceState : uint8
{
	None,
	/** Mutual interest has been acknowledged. */
	Interested,
	/** A mutually agreed romantic relationship. */
	Committed,
	/** An ongoing romance under conflict; may reconcile or separate if the profile allows. */
	Strained,
	/** The romance has ended. */
	Separated
};

UENUM(BlueprintType)
enum class ESlayComparison : uint8
{
	Equal,
	NotEqual,
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual
};

UENUM(BlueprintType)
enum class ESlayConditionType : uint8
{
	/** Compare a relationship dimension (CharacterId/OtherCharacterId, Dimension, Comparison, Value). */
	RelationshipValue,
	/** Relationship romance state equals RomanceState. */
	RomanceState,
	/** The relationship profile allows a romance route at all. */
	RomanceRouteAvailable,
	/** Global persistent narrative flag Name is set. */
	NarrativeFlag,
	/** Relationship-scoped persistent flag Name is set. */
	RelationshipFlag,
	/** Decision Name was made; if Option is not None, that option was chosen. */
	Decision,
	/** Narrative event Name has been marked as seen. */
	EventSeen,
	/** Compare the current chapter index (Comparison, Value). */
	Chapter
};

namespace SlayNarrative
{
	inline bool Compare(int32 Lhs, ESlayComparison Op, int32 Rhs)
	{
		switch (Op)
		{
		case ESlayComparison::Equal:          return Lhs == Rhs;
		case ESlayComparison::NotEqual:       return Lhs != Rhs;
		case ESlayComparison::Less:           return Lhs < Rhs;
		case ESlayComparison::LessOrEqual:    return Lhs <= Rhs;
		case ESlayComparison::Greater:        return Lhs > Rhs;
		case ESlayComparison::GreaterOrEqual: return Lhs >= Rhs;
		}
		return false;
	}

	/** Condition types that depend on story state (choices, flags, events) rather than numbers. */
	inline bool IsStoryConditionType(ESlayConditionType Type)
	{
		return Type == ESlayConditionType::NarrativeFlag
			|| Type == ESlayConditionType::RelationshipFlag
			|| Type == ESlayConditionType::Decision
			|| Type == ESlayConditionType::EventSeen;
	}
}

/**
 * Extension point for prerequisites the built-in condition types cannot express
 * (e.g. character-specific rules). Subclass in C++ or Blueprint and add instances
 * to FSlayNarrativeConditionSet::Custom. Implementations must be deterministic and
 * must not modify narrative state.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class SLAYCREW_API USlayNarrativeCustomCondition : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category="Narrative|Conditions")
	bool IsConditionMet(const USlayNarrativeState* State) const;
	virtual bool IsConditionMet_Implementation(const USlayNarrativeState* State) const { return false; }
};

/** One data-driven condition. Fields not used by the selected Type are ignored. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayNarrativeCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	ESlayConditionType Type = ESlayConditionType::NarrativeFlag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	FName CharacterId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	FName OtherCharacterId = FName(TEXT("Protagonist"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	ESlayRelationshipDimension Dimension = ESlayRelationshipDimension::Trust;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	ESlayComparison Comparison = ESlayComparison::GreaterOrEqual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	int32 Value = 0;

	/** Flag, decision or event ID depending on Type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	FName Name;

	/** Decision: required option ID. None accepts any recorded option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	FName Option;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	ESlayRomanceState RomanceState = ESlayRomanceState::None;

	/** Inverts the result. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	bool bNegate = false;
};

/** Passes when every AllOf condition passes, at least one AnyOf passes (if any), and every Custom passes. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayNarrativeConditionSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	TArray<FSlayNarrativeCondition> AllOf;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Condition")
	TArray<FSlayNarrativeCondition> AnyOf;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category="Condition")
	TArray<TObjectPtr<USlayNarrativeCustomCondition>> Custom;
};

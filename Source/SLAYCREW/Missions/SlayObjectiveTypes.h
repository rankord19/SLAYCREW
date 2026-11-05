#pragma once

#include "CoreMinimal.h"
#include "SlayObjectiveTypes.generated.h"

/** Lifecycle of a single mission objective. Completed and Failed are terminal. */
UENUM(BlueprintType)
enum class ESlayObjectiveState : uint8
{
	Inactive,
	Active,
	Completed,
	Failed
};

/** Static description of an objective, supplied by whoever registers it (mission script, trigger, etc.). */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayObjectiveDefinition
{
	GENERATED_BODY()

	/** Unique key within the world. Must not be None. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	FName ObjectiveId;

	/** Presentation text only; the framework never reads it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	FText DisplayName;

	/** Progress required to complete. Values below 1 are treated as 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective", meta=(ClampMin="1"))
	int32 TargetProgress = 1;

	/** Activate immediately after a successful registration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	bool bActivateOnRegister = false;
};

/** Runtime snapshot of an objective. */
USTRUCT(BlueprintType)
struct SLAYCREW_API FSlayObjectiveStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Objective")
	FSlayObjectiveDefinition Definition;

	UPROPERTY(BlueprintReadOnly, Category="Objective")
	ESlayObjectiveState State = ESlayObjectiveState::Inactive;

	UPROPERTY(BlueprintReadOnly, Category="Objective")
	int32 CurrentProgress = 0;
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Narrative/SlayRelationshipTypes.h"
#include "SlayRelationshipProfileAsset.generated.h"

/** Designer-authored set of relationship profiles, registered via USlayNarrativeSubsystem. */
UCLASS(BlueprintType)
class SLAYCREW_API USlayRelationshipProfileAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Relationships")
	TArray<FSlayRelationshipProfile> Profiles;
};

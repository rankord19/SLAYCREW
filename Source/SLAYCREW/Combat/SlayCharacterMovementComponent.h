#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SlayCharacterMovementComponent.generated.h"

/** Computes temporary speed without overwriting configured base movement stats. */
UCLASS()
class SLAYCREW_API USlayCharacterMovementComponent : public UCharacterMovementComponent
{
 GENERATED_BODY()
public:
 virtual float GetMaxSpeed() const override;
 virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
};

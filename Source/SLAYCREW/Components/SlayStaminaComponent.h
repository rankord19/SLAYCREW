#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlayStaminaComponent.generated.h"
UCLASS(ClassGroup=(SLAYCREW), meta=(BlueprintSpawnableComponent))
class SLAYCREW_API USlayStaminaComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 USlayStaminaComponent();
 virtual void TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stamina") bool TrySpend(float Cost);
 UFUNCTION(BlueprintPure, Category="Stamina") float GetCurrent() const { return Current; }
 UPROPERTY(EditDefaultsOnly, Category="Stamina", meta=(ClampMin="1")) float Maximum = 100.f;
 UPROPERTY(EditDefaultsOnly, Category="Stamina", meta=(ClampMin="0")) float RegenPerSecond = 25.f;
 UPROPERTY(EditDefaultsOnly, Category="Stamina", meta=(ClampMin="0")) float RegenDelay = 1.f;
protected:
 virtual void BeginPlay() override;
private:
 UPROPERTY(Replicated) float Current = 100.f;
 double RegenAt = 0;
};

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlayInjuryComponent.generated.h"

UENUM(BlueprintType)
enum class ESlayInjuryState : uint8 { Healthy, Wounded, Incapacitated };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlayInjuryChanged, ESlayInjuryState, State, float, Severity);

/** Companion presentation and temporary performance modifiers derived from the single HP pool. */
UCLASS(ClassGroup=(SLAYCREW), meta=(BlueprintSpawnableComponent))
class SLAYCREW_API USlayInjuryComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 USlayInjuryComponent();
 UFUNCTION(BlueprintPure, Category="Injury") ESlayInjuryState GetInjuryState() const;
 UFUNCTION(BlueprintPure, Category="Injury") float GetSeverity() const;
 UFUNCTION(BlueprintPure, Category="Injury") float GetMovementMultiplier() const;
 UFUNCTION(BlueprintPure, Category="Injury") float GetAttackSpeedMultiplier() const;
 UFUNCTION(BlueprintPure, Category="Injury") float GetDodgeEffectivenessMultiplier() const;
 UFUNCTION(BlueprintPure, Category="Injury") bool CanAct() const;
 static ESlayInjuryState StateForFraction(float Fraction);
 static float SeverityForFraction(float Fraction);
 UPROPERTY(EditDefaultsOnly, Category="Injury", meta=(ClampMin="0.1", ClampMax="1")) float MinimumMovement = 0.4f;
 UPROPERTY(EditDefaultsOnly, Category="Injury", meta=(ClampMin="0.1", ClampMax="1")) float MinimumAttackSpeed = 0.5f;
 UPROPERTY(EditDefaultsOnly, Category="Injury", meta=(ClampMin="0", ClampMax="1")) float MinimumDodgeEffectiveness = 0.25f;
 /** Fires on every HP change, including changes within Wounded. Use for locomotion, reactions, breathing and audio. */
 UPROPERTY(BlueprintAssignable, Category="Injury") FSlayInjuryChanged OnInjuryChanged;
 UPROPERTY(BlueprintAssignable, Category="Injury") FSlayInjuryChanged OnIncapacitated;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY() TObjectPtr<class USlayHealthComponent> Health;
 ESlayInjuryState LastState = ESlayInjuryState::Healthy;
 UFUNCTION() void HealthChanged(float Previous, float Current);
 float Fraction() const;
 float Multiplier(float Minimum) const;
};

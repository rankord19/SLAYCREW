#include "Combat/SlayInjuryComponent.h"
#include "Components/SlayHealthComponent.h"
#include "Combat/SlayMeleeComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

USlayInjuryComponent::USlayInjuryComponent() { PrimaryComponentTick.bCanEverTick = false; }
void USlayInjuryComponent::BeginPlay()
{
 Super::BeginPlay();
 Health = GetOwner()->FindComponentByClass<USlayHealthComponent>();
 if (Health) { Health->OnHealthChanged.AddUniqueDynamic(this, &USlayInjuryComponent::HealthChanged); }
 HealthChanged(0, 0);
}
void USlayInjuryComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if (Health) { Health->OnHealthChanged.RemoveDynamic(this, &USlayInjuryComponent::HealthChanged); }
 Super::EndPlay(Reason);
}
float USlayInjuryComponent::Fraction() const
{
 return Health && Health->bIncapacitateAtZero ? Health->GetCurrentHealth() / FMath::Max(1.f, Health->GetMaxHealth()) : 1.f;
}
ESlayInjuryState USlayInjuryComponent::StateForFraction(float Fraction)
{
 return Fraction <= 0.f ? ESlayInjuryState::Incapacitated : (Fraction <= 0.5f ? ESlayInjuryState::Wounded : ESlayInjuryState::Healthy);
}
float USlayInjuryComponent::SeverityForFraction(float Fraction) { return FMath::Clamp(1.f - Fraction / 0.5f, 0.f, 1.f); }
ESlayInjuryState USlayInjuryComponent::GetInjuryState() const { return StateForFraction(Fraction()); }
float USlayInjuryComponent::GetSeverity() const { return SeverityForFraction(Fraction()); }
bool USlayInjuryComponent::CanAct() const { return Health && !Health->IsDepleted(); }
float USlayInjuryComponent::Multiplier(float Minimum) const
{
 return CanAct() ? FMath::Lerp(1.f, FMath::Clamp(Minimum, 0.f, 1.f), GetSeverity()) : 0.f;
}
float USlayInjuryComponent::GetMovementMultiplier() const { return Multiplier(MinimumMovement); }
float USlayInjuryComponent::GetAttackSpeedMultiplier() const { return Multiplier(MinimumAttackSpeed); }
float USlayInjuryComponent::GetDodgeEffectivenessMultiplier() const { return Multiplier(MinimumDodgeEffectiveness); }
void USlayInjuryComponent::HealthChanged(float Previous, float Current)
{
 const ESlayInjuryState State = GetInjuryState();
 if (!CanAct())
 {
  if (USlayMeleeComponent* Melee = GetOwner()->FindComponentByClass<USlayMeleeComponent>()) { Melee->CancelAttack(); }
  if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
  {
   Character->StopJumping();
   Character->GetCharacterMovement()->StopMovementImmediately();
   Character->ConsumeMovementInputVector();
  }
 }
 const bool bCollapsed = State == ESlayInjuryState::Incapacitated && LastState != State;
 LastState = State;
 OnInjuryChanged.Broadcast(State, GetSeverity());
 if (bCollapsed) { OnIncapacitated.Broadcast(State, GetSeverity()); }
}

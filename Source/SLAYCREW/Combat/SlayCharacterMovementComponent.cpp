#include "Combat/SlayCharacterMovementComponent.h"
#include "Components/SlayHealthComponent.h"
#include "Combat/SlayInjuryComponent.h"
#include "GameFramework/Character.h"
float USlayCharacterMovementComponent::GetMaxSpeed() const
{
 if (CharacterOwner)
 {
  const USlayHealthComponent* Health = CharacterOwner->FindComponentByClass<USlayHealthComponent>();
  if (Health && Health->IsDepleted()) { return 0.f; }
  const USlayInjuryComponent* Injury = CharacterOwner->FindComponentByClass<USlayInjuryComponent>();
  if (Injury && Injury->HasBegunPlay()) { return Super::GetMaxSpeed() * Injury->GetMovementMultiplier(); }
 }
 return Super::GetMaxSpeed();
}
void USlayCharacterMovementComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
 const USlayHealthComponent* Health = CharacterOwner ? CharacterOwner->FindComponentByClass<USlayHealthComponent>() : nullptr;
 if (Health && Health->IsDepleted())
 {
  StopMovementImmediately();
  CharacterOwner->ConsumeMovementInputVector();
  ClearAccumulatedForces();
  return;
 }
 Super::TickComponent(Delta, TickType, Function);
}

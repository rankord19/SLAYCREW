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
void USlayCharacterMovementComponent::UpdateHealthMovementLock()
{
 const USlayHealthComponent* Health = CharacterOwner ? CharacterOwner->FindComponentByClass<USlayHealthComponent>() : nullptr;
 if (Health && Health->IsDepleted())
 {
  if (!bHealthMovementLocked)
  {
   SavedMovementMode = MovementMode;
   SavedCustomMode = CustomMovementMode;
   bHealthMovementLocked = true;
  }
  StopMovementImmediately();
  CharacterOwner->ConsumeMovementInputVector();
  ClearAccumulatedForces();
  DisableMovement();
 }
 else if (bHealthMovementLocked)
 {
  bHealthMovementLocked = false;
  // Only undo the movement mode owned by this lock. Preserve explicit external modes.
  if (MovementMode == MOVE_None) { SetMovementMode(SavedMovementMode, SavedCustomMode); }
 }
}
void USlayCharacterMovementComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
 UpdateHealthMovementLock();
 // MOVE_None lets the engine process bookkeeping, networking and root-motion cleanup.
 Super::TickComponent(Delta, TickType, Function);
}
void USlayCharacterMovementComponent::PerformMovement(float DeltaSeconds)
{
 UpdateHealthMovementLock(); // Also covers authority processing of client moves outside TickComponent.
 Super::PerformMovement(DeltaSeconds);
}
void USlayCharacterMovementComponent::SimulateMovement(float DeltaSeconds)
{
 UpdateHealthMovementLock();
 Super::SimulateMovement(DeltaSeconds);
}

#include "Combat/SlayComboComponent.h"
#include "Combat/SlayMeleeComponent.h"
#include "Components/SlayHealthComponent.h"
#include "Engine/World.h"
USlayComboComponent::USlayComboComponent() { PrimaryComponentTick.bCanEverTick = true; }
void USlayComboComponent::BeginPlay()
{
 Super::BeginPlay();
 Melee = GetOwner()->FindComponentByClass<USlayMeleeComponent>();
 Health = GetOwner()->FindComponentByClass<USlayHealthComponent>();
 if (Melee) { AddTickPrerequisiteComponent(Melee); Melee->OnAttackCancelled.AddUniqueDynamic(this, &USlayComboComponent::Cancel); }
}
ESlayOpeningDirection USlayComboComponent::Recognize(FVector2D Gesture, float Threshold)
{
 if (Gesture.Size() < FMath::Max(0.f, Threshold) || Gesture.IsNearlyZero()) { return ESlayOpeningDirection::Right; }
 if (FMath::Abs(Gesture.X) >= FMath::Abs(Gesture.Y))
 { return Gesture.X >= 0 ? ESlayOpeningDirection::Right : ESlayOpeningDirection::Left; }
 return Gesture.Y >= 0 ? ESlayOpeningDirection::Up : ESlayOpeningDirection::Down;
}
bool USlayComboComponent::Open(ESlayOpeningDirection Direction)
{
 if (!GetOwner()->HasAuthority() || !Melee || !Health || Health->IsDepleted() || Strike != 0) { return false; }
 bRejected = false;
 if (!Melee->TryStartAttack()) { bRejected = Melee->CanAttack(); return false; }
 Opening = Direction;
 Strike = 1;
 WindowEnds = BufferedUntil = -1;
 return true;
}
void USlayComboComponent::Queue()
{
 if (GetOwner()->HasAuthority() && Strike > 0 && Strike < FMath::Max(1, MaxStrikes) && Health && !Health->IsDepleted())
 { BufferedUntil = GetWorld()->GetTimeSeconds() + FMath::Max(.01f, BufferDuration); }
}
void USlayComboComponent::Cancel()
{
 const bool bHadCombo = Strike > 0;
 Strike = 0;
 WindowEnds = BufferedUntil = -1;
 if (Melee && bHadCombo) { Melee->CancelAttack(); }
}
void USlayComboComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta, Type, Function);
 if (!GetOwner()->HasAuthority() || !Melee || !Health) { return; }
 if (Health->IsDepleted()) { Cancel(); return; }
 if (Strike == 0) { return; }
 const double Now = GetWorld()->GetTimeSeconds();
 if (BufferedUntil < Now) { BufferedUntil = -1; }
 if (Melee->GetAttackState() != ESlayAttackState::Idle) { return; }
 if (Strike >= FMath::Max(1, MaxStrikes)) { Cancel(); return; }
 if (WindowEnds < 0) { WindowEnds = Now + FMath::Max(.01f, FollowupWindow); }
 if (Now > WindowEnds) { Cancel(); return; }
 if (BufferedUntil >= Now)
 {
  BufferedUntil = -1;
  if (Melee->TryStartAttack()) { ++Strike; WindowEnds = -1; bRejected = false; }
  else { bRejected = true; Cancel(); }
 }
}

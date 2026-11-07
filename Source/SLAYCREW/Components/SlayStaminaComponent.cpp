#include "Components/SlayStaminaComponent.h"
#include "Components/SlayHealthComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
USlayStaminaComponent::USlayStaminaComponent()
{
 PrimaryComponentTick.bCanEverTick = true;
 SetIsReplicatedByDefault(true);
}
void USlayStaminaComponent::BeginPlay()
{
 Super::BeginPlay();
 Maximum = FMath::IsFinite(Maximum) ? FMath::Max(1.f, Maximum) : 100.f;
 if (GetOwner()->HasAuthority()) { Current = Maximum; }
}
void USlayStaminaComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(USlayStaminaComponent, Current);
}
bool USlayStaminaComponent::TrySpend(float Cost)
{
 const auto* Health = GetOwner()->FindComponentByClass<USlayHealthComponent>();
 if (!HasBegunPlay() || !GetOwner()->HasAuthority() || !FMath::IsFinite(Cost) || Cost < 0.f
  || Cost > Current || (Health && Health->IsDepleted())) { return false; }
 Current = FMath::Max(0.f, Current - Cost);
 if (Cost > 0) { RegenAt = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, RegenDelay); }
 return true;
}
void USlayStaminaComponent::TickComponent(float Delta, ELevelTick Type, FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta, Type, Function);
 if (!GetOwner()->HasAuthority()) { return; }
 const auto* Health = GetOwner()->FindComponentByClass<USlayHealthComponent>();
 if (Health && Health->IsDepleted()) { return; }
 const float RegenTime = FMath::Clamp(float(GetWorld()->GetTimeSeconds() - RegenAt), 0.f, Delta);
 if (FMath::IsFinite(RegenPerSecond))
 { Current = FMath::Clamp(Current + FMath::Max(0.f, RegenPerSecond) * RegenTime, 0.f, Maximum); }
}

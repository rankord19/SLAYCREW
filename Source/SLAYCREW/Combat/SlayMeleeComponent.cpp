#include "Combat/SlayMeleeComponent.h"
#include "Components/SlayStaminaComponent.h"
#include "Components/SlayHealthComponent.h"
#include "Combat/SlayInjuryComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

USlayMeleeComponent::USlayMeleeComponent()
{
 PrimaryComponentTick.bCanEverTick = true;
 SetIsReplicatedByDefault(true);
}
void USlayMeleeComponent::BeginPlay()
{
 Super::BeginPlay();
 Health = GetOwner()->FindComponentByClass<USlayHealthComponent>();
 Injury = GetOwner()->FindComponentByClass<USlayInjuryComponent>();
 if (Health) { Health->OnHealthChanged.AddUniqueDynamic(this, &USlayMeleeComponent::HealthChanged); }
}
void USlayMeleeComponent::EndPlay(const EEndPlayReason::Type Reason)
{
 if (Health) { Health->OnHealthChanged.RemoveDynamic(this, &USlayMeleeComponent::HealthChanged); }
 Super::EndPlay(Reason);
}
void USlayMeleeComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
 Super::GetLifetimeReplicatedProps(OutLifetimeProps);
 DOREPLIFETIME(USlayMeleeComponent, State);
}
void USlayMeleeComponent::OnRep_State() { OnAttackStateChanged.Broadcast(State); }
void USlayMeleeComponent::SetState(ESlayAttackState NewState)
{
 if (State != NewState) { State = NewState; OnRep_State(); }
}
bool USlayMeleeComponent::CanAttack() const
{
 return HasBegunPlay() && Health && !Health->IsDepleted() && State == ESlayAttackState::Idle
  && GetWorld() && GetWorld()->GetTimeSeconds() >= ReadyAt;
}
void USlayMeleeComponent::RequestAttack()
{
 if (GetOwner()->HasAuthority()) { TryStartAttack(); }
 else if (CanAttack()) { ServerRequestAttack(); }
}
void USlayMeleeComponent::ServerRequestAttack_Implementation() { TryStartAttack(); }
bool USlayMeleeComponent::TryStartAttack()
{
 if (!GetOwner()->HasAuthority() || !CanAttack()) { return false; }
 if (auto* Stamina = GetOwner()->FindComponentByClass<USlayStaminaComponent>())
 { if (!Stamina->TrySpend(StaminaCost)) { return false; } }
 else if (StaminaCost > 0.f) { return false; }
 const float Speed = Injury ? FMath::Clamp(Injury->GetAttackSpeedMultiplier(), 0.1f, 1.f) : 1.f;
 const double Now = GetWorld()->GetTimeSeconds();
 ActiveAt = Now + FMath::Max(0.f, StartupDuration) / Speed;
 AttackEnds = ActiveAt + FMath::Max(0.01f, AttackDuration) / Speed;
 ReadyAt = AttackEnds + FMath::Max(0.01f, Cooldown) / Speed;
 HitActors.Reset();
 SetState(StartupDuration > 0.f ? ESlayAttackState::Startup : ESlayAttackState::Attacking);
 if (!Health->IsDepleted() && State == ESlayAttackState::Attacking) { Sweep(); }
 return true;
}
void USlayMeleeComponent::CancelAttack()
{
 if (!GetOwner() || !GetOwner()->HasAuthority()) { return; }
 if (State == ESlayAttackState::Attacking || State == ESlayAttackState::Startup)
 { SetState(ESlayAttackState::Cooldown); }
 OnAttackCancelled.Broadcast();
}
void USlayMeleeComponent::HealthChanged(float Previous, float Current) { if (Current <= 0) { CancelAttack(); } }
void USlayMeleeComponent::TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function)
{
 Super::TickComponent(Delta, TickType, Function);
 if (!GetOwner()->HasAuthority()) { return; }
 const double Now = GetWorld()->GetTimeSeconds();
 if (State == ESlayAttackState::Startup && Now >= ActiveAt)
 {
  SetState(Health && !Health->IsDepleted() ? ESlayAttackState::Attacking : ESlayAttackState::Cooldown);
 }
 if (State == ESlayAttackState::Attacking)
 {
  if (!Health || Health->IsDepleted() || Now >= AttackEnds) { SetState(ESlayAttackState::Cooldown); }
  else { Sweep(); }
 }
 if (State == ESlayAttackState::Cooldown && Now >= ReadyAt) { SetState(ESlayAttackState::Idle); }
}
bool USlayMeleeComponent::CanHitTarget_Implementation(AActor* Target) const
{
 if (!IsValid(Target) || Target == GetOwner() || !Target->CanBeDamaged()) { return false; }
 const USlayHealthComponent* TargetHealth = Target->FindComponentByClass<USlayHealthComponent>();
 return TargetHealth && (!TargetHealth->IsDepleted() || bAllowDepletedTargets)
  && (bAllowFriendlyFire || !IsFriendlyTarget(Target));
}
bool USlayMeleeComponent::IsFriendlyTarget_Implementation(AActor* Target) const
{
 const USlayMeleeComponent* Other = IsValid(Target) ? Target->FindComponentByClass<USlayMeleeComponent>() : nullptr;
 return Other && !CombatGroup.IsNone() && CombatGroup == Other->CombatGroup;
}
void USlayMeleeComponent::Sweep()
{
 if (State != ESlayAttackState::Attacking || !Health || Health->IsDepleted()) { return; }
 const FVector Start = GetOwner()->GetActorLocation();
 const FVector End = Start + GetOwner()->GetActorForwardVector() * FMath::Clamp(Reach, 1.f, 1000.f);
 FCollisionQueryParams Params(SCENE_QUERY_STAT(SlayMelee), false, GetOwner());
 FCollisionObjectQueryParams Objects;
 Objects.AddObjectTypesToQuery(ECC_Pawn);
 Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 TArray<FHitResult> Hits;
 GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects,
  FCollisionShape::MakeSphere(FMath::Clamp(Radius, 1.f, 200.f)), Params);
 for (const FHitResult& Hit : Hits)
 {
  if (State != ESlayAttackState::Attacking || Health->IsDepleted()) { break; }
  AActor* Target = Hit.GetActor();
  if (!CanHitTarget(Target) || HitActors.Contains(Target)) { continue; }
  // Visibility occlusion prevents attacks through walls. Ignore the target's own collision.
  FCollisionQueryParams SightParams = Params;
  SightParams.AddIgnoredActor(Target);
  FHitResult Obstruction;
  if (GetWorld()->LineTraceSingleByChannel(Obstruction, Start, Target->GetActorLocation(), ECC_Visibility, SightParams)) { continue; }
  HitActors.Add(Target); // Before damage callbacks: reentrant events cannot damage the same target twice.
  LastHit = Hit;
  ++HitSerial;
  const APawn* Pawn = Cast<APawn>(GetOwner());
  UGameplayStatics::ApplyDamage(Target, Damage, Pawn ? Pawn->GetController() : nullptr, GetOwner(), nullptr);
 }
}

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlayMeleeComponent.generated.h"

UENUM(BlueprintType)
enum class ESlayAttackState : uint8 { Idle, Attacking, Cooldown };
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSlayAttackChanged, ESlayAttackState, State);

/** Server-owned short sweep window. Clients request attacks, never supply hits or damage. */
UCLASS(ClassGroup=(SLAYCREW), meta=(BlueprintSpawnableComponent))
class SLAYCREW_API USlayMeleeComponent : public UActorComponent
{
 GENERATED_BODY()
public:
 USlayMeleeComponent();
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
 virtual void TickComponent(float Delta, ELevelTick TickType, FActorComponentTickFunction* Function) override;
 UFUNCTION(BlueprintCallable, Category="Combat") void RequestAttack();
 UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Combat") bool TryStartAttack();
 UFUNCTION(BlueprintCallable, Category="Combat") void CancelAttack();
 UFUNCTION(BlueprintPure, Category="Combat") bool CanAttack() const;
 UFUNCTION(BlueprintPure, Category="Combat") ESlayAttackState GetAttackState() const { return State; }
 /** Matching non-None groups are allies. None is unaffiliated. Configure identical defaults on peers. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Eligibility") FName CombatGroup = NAME_None;
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Eligibility") bool bAllowFriendlyFire = false;
 /** Eligibility only; does not change depleted health's damage/recovery rules. */
 UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat|Eligibility") bool bAllowDepletedTargets = false;
 UFUNCTION(BlueprintNativeEvent, Category="Combat|Eligibility") bool IsFriendlyTarget(AActor* Target) const;
 /** Override for teams, PvP rules or weapon-specific target filters. Runs on authority. */
 UFUNCTION(BlueprintNativeEvent, Category="Combat") bool CanHitTarget(AActor* Target) const;
 UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0")) float Damage = 25.f;
 UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float Reach = 150.f;
 UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="1")) float Radius = 35.f;
 UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.01")) float AttackDuration = 0.15f;
 UPROPERTY(EditDefaultsOnly, Category="Combat", meta=(ClampMin="0.01")) float Cooldown = 0.45f;
 UPROPERTY(BlueprintAssignable, Category="Combat") FSlayAttackChanged OnAttackStateChanged;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
 UPROPERTY(ReplicatedUsing=OnRep_State) ESlayAttackState State = ESlayAttackState::Idle;
 UPROPERTY() TObjectPtr<class USlayHealthComponent> Health;
 UPROPERTY() TObjectPtr<class USlayInjuryComponent> Injury;
 TSet<TWeakObjectPtr<AActor>> HitActors;
 double AttackEnds = 0;
 double ReadyAt = 0;
 UFUNCTION(Server, Reliable) void ServerRequestAttack();
 UFUNCTION() void OnRep_State();
 UFUNCTION() void HealthChanged(float Previous, float Current);
 void SetState(ESlayAttackState NewState);
 void Sweep();
};

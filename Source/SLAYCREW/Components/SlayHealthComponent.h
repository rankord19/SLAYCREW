#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SlayHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSlayHealthChanged, float, PreviousHealth, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlayHealthDeath);

/** Server-owned health. The owning actor must replicate for clients to receive state. */
UCLASS(ClassGroup=(SLAYCREW), meta=(BlueprintSpawnableComponent))
class SLAYCREW_API USlayHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USlayHealthComponent();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return IsDepleted() && !bIncapacitateAtZero; }

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDepleted() const { return CurrentHealth <= 0.0f; }

	/** Companion policy. Configure in defaults, never change during play. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category="Health")
	bool bIncapacitateAtZero = false;

	UPROPERTY(EditDefaultsOnly, Category="Health")
	bool bAllowHealingRecovery = false;

	/** Explicit authority-only recovery; never revives a mortal actor. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	bool RecoverFromIncapacitation(float HealthFraction);

	/** Returns actual health removed. Invalid amounts and non-authority calls do nothing. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	float ApplyDamage(float Amount);

	/** Ordinary healing cannot recover incapacitated companions unless explicitly enabled. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Health")
	float Heal(float Amount);

	UPROPERTY(BlueprintAssignable, Category="Health")
	FSlayHealthChanged OnHealthChanged;

	/** Authority: once on death. Clients: when replicated health first crosses zero. */
	UPROPERTY(BlueprintAssignable, Category="Health")
	FSlayHealthDeath OnDeath;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Initial capacity; sanitized at BeginPlay. Runtime capacity changes are not supported. */
	UPROPERTY(EditDefaultsOnly, Replicated, Category="Health", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(ReplicatedUsing=OnRep_CurrentHealth)
	float CurrentHealth = 100.0f;

	UFUNCTION()
	void OnRep_CurrentHealth(float PreviousHealth);

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		class AController* InstigatedBy, AActor* DamageCauser);

	bool CanChangeHealth(float Amount) const;
	void NotifyHealthChanged(float PreviousHealth);
};

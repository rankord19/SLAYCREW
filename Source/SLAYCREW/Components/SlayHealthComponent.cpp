#include "Components/SlayHealthComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

USlayHealthComponent::USlayHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USlayHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(1.0f, MaxHealth) : 100.0f;
		const float PreviousHealth = CurrentHealth;
		CurrentHealth = MaxHealth;
		GetOwner()->OnTakeAnyDamage.AddUniqueDynamic(this, &USlayHealthComponent::HandleAnyDamage);
		NotifyHealthChanged(PreviousHealth);
	}
}

void USlayHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner())
	{
		GetOwner()->OnTakeAnyDamage.RemoveDynamic(this, &USlayHealthComponent::HandleAnyDamage);
	}
	Super::EndPlay(EndPlayReason);
}

void USlayHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(USlayHealthComponent, MaxHealth);
	DOREPLIFETIME(USlayHealthComponent, CurrentHealth);
}

bool USlayHealthComponent::CanChangeHealth(float Amount) const
{
	return HasBegunPlay() && GetOwner() && GetOwner()->HasAuthority()
		&& FMath::IsFinite(Amount) && Amount > 0.0f && !IsDead();
}

float USlayHealthComponent::ApplyDamage(float Amount)
{
	if (!CanChangeHealth(Amount))
	{
		return 0.0f;
	}
	const float PreviousHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth - FMath::Min(Amount, CurrentHealth), 0.0f, MaxHealth);
	const float Applied = PreviousHealth - CurrentHealth;
	NotifyHealthChanged(PreviousHealth);
	return Applied;
}

float USlayHealthComponent::Heal(float Amount)
{
	if (!CanChangeHealth(Amount))
	{
		return 0.0f;
	}
	const float PreviousHealth = CurrentHealth;
	// Limit before addition so even a finite FLT_MAX input cannot overflow.
	CurrentHealth = FMath::Clamp(CurrentHealth + FMath::Min(Amount, MaxHealth - CurrentHealth), 0.0f, MaxHealth);
	const float Applied = CurrentHealth - PreviousHealth;
	NotifyHealthChanged(PreviousHealth);
	return Applied;
}

void USlayHealthComponent::HandleAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	ApplyDamage(Damage);
}

void USlayHealthComponent::OnRep_CurrentHealth(float PreviousHealth)
{
	NotifyHealthChanged(PreviousHealth);
}

void USlayHealthComponent::NotifyHealthChanged(float PreviousHealth)
{
	const float NewHealth = CurrentHealth;
	if (PreviousHealth != NewHealth)
	{
		OnHealthChanged.Broadcast(PreviousHealth, NewHealth);
		if (PreviousHealth > 0.0f && NewHealth <= 0.0f)
		{
			OnDeath.Broadcast();
		}
	}
}

#pragma once
#include "CoreMinimal.h"
#include "SLAYCREWCharacter.h"
#include "SlayFighterCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSlayProtagonistDeath);

/** Reparent the third-person player Blueprint to this class to enable the combat foundation. */
UCLASS()
class SLAYCREW_API ASlayFighterCharacter : public ASLAYCREWCharacter
{
 GENERATED_BODY()
public:
 ASlayFighterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<class USlayHealthComponent> Health;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<class USlayMeleeComponent> Melee;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") TObjectPtr<class USlayInjuryComponent> Injury;
 UPROPERTY(BlueprintAssignable, Category="Combat") FSlayProtagonistDeath OnProtagonistDeath;
 /** Single presentation hook for animation, sound and local Game Over UI. */
 UFUNCTION(BlueprintImplementableEvent, Category="Combat") void ReceiveProtagonistDeath();
 UFUNCTION(BlueprintCallable, Category="Combat") void BasicAttack();
 UFUNCTION(BlueprintPure, Category="Combat") bool CanPerformActions() const;
 virtual void DoMove(float Right, float Forward) override;
 virtual void DoLook(float Yaw, float Pitch) override;
 virtual void Jump() override;
 virtual void UnPossessed() override;
 virtual void OnRep_Controller() override;
protected:
 virtual void BeginPlay() override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
 /** Optional asset action already mapped by the player controller. Null uses transient LMB/gamepad mapping. */
 UPROPERTY(EditDefaultsOnly, Category="Input") TObjectPtr<class UInputAction> BasicAttackAction;
private:
 UPROPERTY(Transient) TObjectPtr<class UInputAction> DefaultAttackAction;
 UPROPERTY(Transient) TObjectPtr<class UInputMappingContext> AttackMapping;
 TWeakObjectPtr<class UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
 bool bDeathHandled = false;
 UFUNCTION() void HandleDeath();
 void RemoveAttackMapping();
};

/** Same foundation for each of the four companions; zero HP means incapacitation, never combat death. */
UCLASS()
class SLAYCREW_API ASlayCompanionCharacter : public ASlayFighterCharacter
{
 GENERATED_BODY()
public:
 ASlayCompanionCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

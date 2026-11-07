#pragma once
#include "CoreMinimal.h"
#include "Combat/SlayFighterCharacter.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "SlayPrototype.generated.h"
/** Standalone keyboard/mouse harness. Does not replace the existing content player. */
UCLASS()
class SLAYCREW_API ASlayPrototypeCharacter : public ASlayFighterCharacter
{
 GENERATED_BODY()
public:
 ASlayPrototypeCharacter();
 virtual void Tick(float Delta) override;
 virtual void UnPossessed() override;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype") TObjectPtr<class USlayStaminaComponent> Stamina;
 UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Prototype") TObjectPtr<class USlayComboComponent> Combo;
 UPROPERTY(VisibleAnywhere, Category="Prototype") TObjectPtr<class UStaticMeshComponent> Sword;
 UPROPERTY(EditDefaultsOnly, Category="Prototype") float WalkSpeed = 380.f;
 UPROPERTY(EditDefaultsOnly, Category="Prototype") float SprintSpeed = 620.f;
 UPROPERTY(EditDefaultsOnly, Category="Prototype") float SprintCostPerSecond = 22.f;
 UPROPERTY(EditDefaultsOnly, Category="Prototype") float AttackMoveSpeed = 140.f;
 UPROPERTY(EditDefaultsOnly, Category="Prototype") float AttackTurnRate = 90.f;
 UPROPERTY(EditDefaultsOnly, Category="Prototype") float DummyAttackInterval = 2.f;
 UPROPERTY(EditAnywhere, Category="Prototype") bool bTimedAttacker = false;
 UPROPERTY(BlueprintReadOnly, Category="Prototype") bool bSelecting = false;
 UPROPERTY(BlueprintReadOnly, Category="Prototype") bool bSprintRejected = false;
protected:
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
private:
 FVector2D Gesture = FVector2D::ZeroVector;
 float SwingTime = 0;
 double NextDummyAttack = 2;
};
UCLASS()
class SLAYCREW_API ASlayPrototypeGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 ASlayPrototypeGameMode();
 virtual void InitGame(const FString& Map, const FString& Options, FString& Error) override;
 virtual void RestartPlayer(AController* Player) override;
};
UCLASS()
class SLAYCREW_API ASlayPrototypeHUD : public AHUD
{
 GENERATED_BODY()
public:
 virtual void DrawHUD() override;
};

#include "Combat/SlayFighterCharacter.h"
#include "Combat/SlayCharacterMovementComponent.h"
#include "Combat/SlayMeleeComponent.h"
#include "Combat/SlayInjuryComponent.h"
#include "Components/SlayHealthComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"

ASlayFighterCharacter::ASlayFighterCharacter(const FObjectInitializer& ObjectInitializer)
 : Super(ObjectInitializer.SetDefaultSubobjectClass<USlayCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
 bReplicates = true;
 Health = CreateDefaultSubobject<USlayHealthComponent>(TEXT("Health"));
 Melee = CreateDefaultSubobject<USlayMeleeComponent>(TEXT("Melee"));
 Injury = CreateDefaultSubobject<USlayInjuryComponent>(TEXT("Injury"));
}
ASlayCompanionCharacter::ASlayCompanionCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
 Health->bIncapacitateAtZero = true;
}
void ASlayFighterCharacter::BeginPlay()
{
 Super::BeginPlay();
 Health->OnDeath.AddUniqueDynamic(this, &ASlayFighterCharacter::HandleDeath);
 if (Health->IsDead()) { HandleDeath(); }
}
void ASlayFighterCharacter::RemoveAttackMapping()
{
 if (MappingSubsystem.IsValid() && AttackMapping) { MappingSubsystem->RemoveMappingContext(AttackMapping); }
 MappingSubsystem.Reset();
}
void ASlayFighterCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
 RemoveAttackMapping();
 Health->OnDeath.RemoveDynamic(this, &ASlayFighterCharacter::HandleDeath);
 Super::EndPlay(Reason);
}
void ASlayFighterCharacter::UnPossessed()
{
 RemoveAttackMapping();
 Super::UnPossessed();
}
void ASlayFighterCharacter::OnRep_Controller()
{
 if (!GetController()) { RemoveAttackMapping(); }
 Super::OnRep_Controller();
}
void ASlayFighterCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
 Super::SetupPlayerInputComponent(Input);
 RemoveAttackMapping();
 if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(Input))
 {
  UInputAction* Action = BasicAttackAction;
  if (!Action)
  {
   if (!DefaultAttackAction)
   {
    DefaultAttackAction = NewObject<UInputAction>(this);
    DefaultAttackAction->ValueType = EInputActionValueType::Boolean;
    AttackMapping = NewObject<UInputMappingContext>(this);
    AttackMapping->MapKey(DefaultAttackAction, EKeys::LeftMouseButton);
    AttackMapping->MapKey(DefaultAttackAction, EKeys::Gamepad_RightShoulder);
   }
   Action = DefaultAttackAction;
   if (APlayerController* PC = Cast<APlayerController>(GetController()))
   {
    if (ULocalPlayer* Local = PC->GetLocalPlayer())
    {
     MappingSubsystem = Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
     if (MappingSubsystem.IsValid()) { MappingSubsystem->AddMappingContext(AttackMapping, 0); }
    }
   }
  }
  Enhanced->BindAction(Action, ETriggerEvent::Started, this, &ASlayFighterCharacter::BasicAttack);
 }
}
bool ASlayFighterCharacter::CanPerformActions() const { return !bDeathHandled && Health && !Health->IsDepleted(); }
void ASlayFighterCharacter::BasicAttack() { if (CanPerformActions()) { Melee->RequestAttack(); } }
void ASlayFighterCharacter::DoMove(float Right, float Forward) { if (CanPerformActions()) { Super::DoMove(Right, Forward); } }
void ASlayFighterCharacter::DoLook(float Yaw, float Pitch) { if (CanPerformActions()) { Super::DoLook(Yaw, Pitch); } }
void ASlayFighterCharacter::Jump() { if (CanPerformActions()) { Super::Jump(); } }
void ASlayFighterCharacter::HandleDeath()
{
 if (bDeathHandled || !Health->IsDead()) { return; }
 bDeathHandled = true;
 Melee->CancelAttack();
 StopJumping();
 ConsumeMovementInputVector();
 GetCharacterMovement()->StopMovementImmediately();
 GetCharacterMovement()->DisableMovement();
 OnProtagonistDeath.Broadcast();
 ReceiveProtagonistDeath();
}

#include "Prototype/SlayPrototype.h"
#include "Combat/SlayComboComponent.h"
#include "Combat/SlayMeleeComponent.h"
#include "Components/SlayStaminaComponent.h"
#include "Components/SlayHealthComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"
#include "InputCoreTypes.h"

ASlayPrototypeCharacter::ASlayPrototypeCharacter()
{
 PrimaryActorTick.bCanEverTick = true;
 Stamina = CreateDefaultSubobject<USlayStaminaComponent>(TEXT("Stamina"));
 Combo = CreateDefaultSubobject<USlayComboComponent>(TEXT("Combo"));
 Melee->StartupDuration = .18f;
 Melee->AttackDuration = .18f;
 Melee->Cooldown = .32f;
 Melee->StaminaCost = 28.f;
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
 static ConstructorHelpers::FClassFinder<UAnimInstance> Animation(TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
 if (Body.Succeeded()) { GetMesh()->SetSkeletalMesh(Body.Object); }
 if (Animation.Succeeded()) { GetMesh()->SetAnimInstanceClass(Animation.Class); }
 GetMesh()->SetRelativeLocation(FVector(0, 0, -96));
 GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
 GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Sword = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderSword"));
 Sword->SetupAttachment(RootComponent);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube"));
 Sword->SetStaticMesh(Cube.Object);
 Sword->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Sword->SetRelativeScale3D(FVector(1.25f, .055f, .09f));
 Sword->SetRelativeLocation(FVector(65, 40, 0));
}
void ASlayPrototypeCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
 // This isolated standalone harness polls hardware below. Do not install the base
 // fighter's immediate-LMB action, which would bypass gesture/combination input.
 check(Input);
}
void ASlayPrototypeCharacter::UnPossessed()
{
 bSelecting = false;
 Combo->Cancel();
 Super::UnPossessed();
}
void ASlayPrototypeCharacter::Tick(float Delta)
{
 Super::Tick(Delta);
 const bool bBusy = Melee->GetAttackState() != ESlayAttackState::Idle;
 SwingTime = bBusy ? SwingTime + Delta : 0.f;
 // Temporary procedural swing, not an authored directional sword animation.
 const float SweepAngle = bBusy ? FMath::Sin(SwingTime * 12.f) * 65.f : 0.f;
 const bool bVertical = Combo->Opening == ESlayOpeningDirection::Up || Combo->Opening == ESlayOpeningDirection::Down;
 const float Sign = Combo->Opening == ESlayOpeningDirection::Left || Combo->Opening == ESlayOpeningDirection::Down ? -1.f : 1.f;
 Sword->SetRelativeRotation(bVertical ? FRotator(SweepAngle * Sign, 0, 0) : FRotator(0, SweepAngle * Sign, 0));
 if (!CanPerformActions()) { bSelecting = false; Combo->Cancel(); return; }
 if (!IsPlayerControlled())
 {
  // Stationary timed attacker: no navigation, pursuit, target snapping or Argon AI.
  if (bTimedAttacker && HasAuthority() && GetWorld()->GetTimeSeconds() >= NextDummyAttack)
  {
   Melee->TryStartAttack();
   NextDummyAttack = GetWorld()->GetTimeSeconds() + FMath::Max(.1f, DummyAttackInterval);
  }
  return;
 }
 APlayerController* PC = Cast<APlayerController>(Controller);
 if (!PC || !IsLocallyControlled() || !HasAuthority()) { return; }
 if (PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
 {
  if (Combo->Strike > 0) { Combo->Queue(); }
  else if (Melee->CanAttack()) { bSelecting = true; Gesture = FVector2D::ZeroVector; }
 }
 float MouseX = 0, MouseY = 0;
 PC->GetInputMouseDelta(MouseX, MouseY);
 if (bSelecting) { Gesture += FVector2D(MouseX, MouseY); }
 else { DoLook(MouseX, -MouseY); }
 if (bSelecting && !PC->IsInputKeyDown(EKeys::LeftMouseButton))
 {
  bSelecting = false;
  Combo->Open(USlayComboComponent::Recognize(Gesture, Combo->GestureThreshold));
 }
 const float Forward = float(PC->IsInputKeyDown(EKeys::W)) - float(PC->IsInputKeyDown(EKeys::S));
 const float Right = float(PC->IsInputKeyDown(EKeys::D)) - float(PC->IsInputKeyDown(EKeys::A));
 const bool bAttack = Melee->GetAttackState() != ESlayAttackState::Idle;
 bool bSprint = !bAttack && (Forward != 0 || Right != 0) && PC->IsInputKeyDown(EKeys::LeftShift);
 bSprintRejected = bSprint && !Stamina->TrySpend(FMath::Max(0.f, SprintCostPerSecond) * Delta);
 bSprint &= !bSprintRejected;
 GetCharacterMovement()->MaxWalkSpeed = bAttack ? AttackMoveSpeed : (bSprint ? SprintSpeed : WalkSpeed);
 GetCharacterMovement()->RotationRate.Yaw = bAttack ? AttackTurnRate : 500.f;
 DoMove(Right, Forward);
 if (PC->WasInputKeyJustPressed(EKeys::SpaceBar) && !bAttack) { Jump(); }
 if (PC->WasInputKeyJustReleased(EKeys::SpaceBar)) { StopJumping(); }
}
ASlayPrototypeGameMode::ASlayPrototypeGameMode()
{
 DefaultPawnClass = ASlayPrototypeCharacter::StaticClass();
 PlayerControllerClass = APlayerController::StaticClass();
 HUDClass = ASlayPrototypeHUD::StaticClass();
}
void ASlayPrototypeGameMode::InitGame(const FString& Map, const FString& Options, FString& Error)
{
 Super::InitGame(Map, Options, Error);
 UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
 auto Block = [this, Cube](FVector Location, FVector Scale)
 {
  auto* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator);
  Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
  Actor->SetActorScale3D(Scale);
  Actor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
 };
 Block(FVector(0, 0, -50), FVector(20, 20, 1));
 Block(FVector(0, -1000, 100), FVector(20, .2, 3));
 Block(FVector(0, 1000, 100), FVector(20, .2, 3));
 Block(FVector(-1000, 0, 100), FVector(.2, 20, 3));
 Block(FVector(1000, 0, 100), FVector(.2, 20, 3));
 auto* Light = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 600), FRotator(-55, -35, 0));
 Light->GetLightComponent()->SetIntensity(5.f);
 FActorSpawnParameters Params;
 Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Enemy = GetWorld()->SpawnActor<ASlayPrototypeCharacter>(FVector(250, 0, 100), FRotator(0, 180, 0), Params);
 Enemy->bTimedAttacker = true;
 Enemy->Melee->CombatGroup = TEXT("PrototypeEnemy");
 Enemy->Melee->Damage = 15.f;
 Enemy->Melee->StartupDuration = .5f;
 Enemy->Melee->StaminaCost = 0.f;
}
void ASlayPrototypeGameMode::RestartPlayer(AController* Player)
{
 RestartPlayerAtTransform(Player, FTransform(FRotator::ZeroRotator, FVector(-100, 0, 100)));
}
void ASlayPrototypeHUD::DrawHUD()
{
 Super::DrawHUD();
 auto* Player = Cast<ASlayPrototypeCharacter>(GetOwningPawn());
 if (!Player || !Canvas) { return; }
 DrawRect(FLinearColor(0, 0, 0, .75f), 12, 12, 680, 185);
 DrawText(FString::Printf(TEXT("HP %.0f / %.0f     STAMINA %.0f / %.0f"),
  Player->Health->GetCurrentHealth(), Player->Health->GetMaxHealth(), Player->Stamina->GetCurrent(), Player->Stamina->Maximum), FLinearColor::White, 24, 22);
 DrawText(TEXT("WASD move | Mouse camera | Shift sprint | Space jump"), FLinearColor::White, 24, 48);
 DrawText(TEXT("Hold LMB + gesture, release to strike. Press again near recovery for combo (max 3)."), FLinearColor::White, 24, 70);
 DrawText(FString::Printf(TEXT("Strike %d | Opening %s | %s | Hits %d"), Player->Combo->Strike,
  *UEnum::GetValueAsString(Player->Combo->Opening), Player->bSelecting ? TEXT("SELECTING") : *UEnum::GetValueAsString(Player->Melee->GetAttackState()), Player->Melee->HitSerial), FLinearColor::Yellow, 24, 94);
 if (Player->Combo->bRejected || Player->bSprintRejected)
 { DrawText(TEXT("INSUFFICIENT STAMINA - wait for regeneration"), FLinearColor::Red, 24, 118); }
 for (TActorIterator<ASlayPrototypeCharacter> It(GetWorld()); It; ++It)
 {
  if (*It == Player) { continue; }
  DrawText(FString::Printf(TEXT("Enemy HP %.0f | %s"), It->Health->GetCurrentHealth(),
   It->Health->IsDead() ? TEXT("DEFEATED") : *UEnum::GetValueAsString(It->Melee->GetAttackState())), FLinearColor::Yellow, 24, 142);
 }
 DrawText(TEXT("Prototype: procedural sword placeholder; standalone only. Esc stops PIE."), FLinearColor::White, 24, 166);
 if (Player->Health->IsDead()) { DrawText(TEXT("GAME OVER - Stop and Play to restart"), FLinearColor::Red, 180, 240, nullptr, 2.f); }
}

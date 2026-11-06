#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Combat/SlayFighterCharacter.h"
#include "Combat/SlayMeleeComponent.h"
#include "Combat/SlayInjuryComponent.h"
#include "Components/SlayHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Tests/SlayCombatTestObserver.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"

namespace SlayTests
{
struct FWorld
{
 UWorld* World;
 FWorld()
 {
  const UWorld::InitializationValues Values = UWorld::InitializationValues()
   .CreatePhysicsScene(true).ShouldSimulatePhysics(true).EnableTraceCollision(true)
   .CreateNavigation(false).CreateAISystem(false).AllowAudioPlayback(false);
  World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
  GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
  World->SetGameInstance(NewObject<UGameInstance>(GEngine));
  FURL URL;
  URL.AddOption(TEXT("game=/Script/Engine.GameModeBase"));
  World->SetGameMode(URL); // APawn::ShouldTakeDamage requires an authority GameMode.
  World->InitializeActorsForPlay(URL);
 }
 ~FWorld() { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); }
 template<class T> T* Spawn(FVector Location = FVector::ZeroVector)
 {
  FActorSpawnParameters Params;
  Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  T* Actor = World->SpawnActor<T>(Location, FRotator::ZeroRotator, Params);
  if (ACharacter* Character = Cast<ACharacter>(Actor))
  {
   // These fixtures are unpossessed; explicitly select walking to test speed modifiers.
   Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
  }
  return Actor;
 }
 void Advance(float Seconds)
 {
  if (!World->HasBegunPlay()) { World->BeginPlay(); }
  for (float Time = 0; Time < Seconds; Time += 0.02f)
  {
   ++GFrameCounter; // Tick prerequisites are deduplicated by engine frame, not World::Tick calls.
   World->Tick(LEVELTICK_All, 0.02f);
  }
 }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayCombatTest, "SLAYCREW.Combat.SweepCooldownAndDeath",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlayCombatTest::RunTest(const FString& Parameters)
{
 SlayTests::FWorld Fixture;
 ASlayFighterCharacter* Player = Fixture.Spawn<ASlayFighterCharacter>();
 ASlayFighterCharacter* Target = Fixture.Spawn<ASlayFighterCharacter>(FVector(120, 0, 0));
 Target->Melee->CombatGroup = TEXT("Hostile");
 Player->DispatchBeginPlay();
 Target->DispatchBeginPlay();
 // Multiple target primitives must still produce only one damage application.
 UBoxComponent* Extra = NewObject<UBoxComponent>(Target);
 Target->AddInstanceComponent(Extra);
 Extra->SetupAttachment(Target->GetRootComponent());
 Extra->SetBoxExtent(FVector(25));
 Extra->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Extra->SetCollisionObjectType(ECC_Pawn);
 Extra->RegisterComponent();
 Fixture.Advance(0.02f); // Publish newly registered collision to the physics scene.
 TestTrue(TEXT("Target accepted by combat filter"), Player->Melee->CanHitTarget(Target));
 UGameplayStatics::ApplyDamage(Target, 1.f, nullptr, Player, nullptr);
 TestEqual(TEXT("Character routes engine damage"), Target->Health->GetCurrentHealth(), 99.f);
 Target->Health->Heal(1.f);
 TArray<FHitResult> ProbeHits;
 FCollisionObjectQueryParams ProbeObjects;
 ProbeObjects.AddObjectTypesToQuery(ECC_Pawn);
 Fixture.World->SweepMultiByObjectType(ProbeHits, Player->GetActorLocation(), Player->GetActorLocation() + FVector(150,0,0),
  FQuat::Identity, ProbeObjects, FCollisionShape::MakeSphere(35), FCollisionQueryParams(NAME_None, false, Player));
 TestTrue(TEXT("Fixture target collision is queryable"), ProbeHits.Num() > 0);
 TestTrue(TEXT("First attack accepted"), Player->Melee->TryStartAttack());
 TestEqual(TEXT("Sweep applies damage via health"), Target->Health->GetCurrentHealth(), 75.f);
 TestFalse(TEXT("Attack spam rejected"), Player->Melee->TryStartAttack());
 Fixture.Advance(0.1f);
 TestEqual(TEXT("Repeated sweeps and primitives deduplicated"), Target->Health->GetCurrentHealth(), 75.f);
 Fixture.Advance(0.12f);
 TestEqual(TEXT("Recovery state"), Player->Melee->GetAttackState(), ESlayAttackState::Cooldown);
 TestFalse(TEXT("Cooldown rejected"), Player->Melee->TryStartAttack());
 Fixture.Advance(0.5f);
 TestTrue(TEXT("Cooldown expires"), Player->Melee->TryStartAttack());
 TestEqual(TEXT("New attack may hit same actor"), Target->Health->GetCurrentHealth(), 50.f);
 USlayCombatTestObserver* Observer = NewObject<USlayCombatTestObserver>(Player);
 Player->OnProtagonistDeath.AddDynamic(Observer, &USlayCombatTestObserver::Death);
 Player->Health->ApplyDamage(1000.f);
 Player->Health->ApplyDamage(1.f);
 TestEqual(TEXT("Death broadcast exactly once"), Observer->Deaths, 1);
 TestTrue(TEXT("Dead immediately"), Player->Health->IsDead());
 TestFalse(TEXT("Actions disabled"), Player->CanPerformActions());
 TestFalse(TEXT("Dead attack rejected"), Player->Melee->TryStartAttack());
 TestTrue(TEXT("Attack canceled"), Player->Melee->GetAttackState() != ESlayAttackState::Attacking);
 TestEqual(TEXT("Movement disabled"), Player->GetCharacterMovement()->MovementMode, MOVE_None);
 Player->DoMove(1, 1);
 Player->Jump();
 TestTrue(TEXT("No movement input while dead"), Player->GetPendingMovementInputVector().IsZero());
 TestFalse(TEXT("No jump while dead"), Player->bPressedJump);
 TestEqual(TEXT("Death is not healable"), Player->Health->Heal(100.f), 0.f);
 TestFalse(TEXT("Companion recovery cannot revive protagonist"), Player->Health->RecoverFromIncapacitation(1.f));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayInjuryTest, "SLAYCREW.Combat.CompanionInjuryAndRecovery",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlayInjuryTest::RunTest(const FString& Parameters)
{
 SlayTests::FWorld Fixture;
 ASlayCompanionCharacter* Companion = Fixture.Spawn<ASlayCompanionCharacter>();
 // Non-100 capacity verifies percentage-based behavior without adding runtime capacity mutation.
 FFloatProperty* Capacity = FindFProperty<FFloatProperty>(USlayHealthComponent::StaticClass(), TEXT("MaxHealth"));
 Capacity->SetPropertyValue_InContainer(Companion->Health, 240.f);
 Companion->DispatchBeginPlay();
 USlayCombatTestObserver* Observer = NewObject<USlayCombatTestObserver>(Companion);
 Companion->Health->OnDeath.AddDynamic(Observer, &USlayCombatTestObserver::Death);
 Companion->Injury->OnIncapacitated.AddDynamic(Observer, &USlayCombatTestObserver::Collapse);
 const float BaseSpeed = Companion->GetCharacterMovement()->MaxWalkSpeed;
 TestEqual(TEXT("Healthy initially"), Companion->Injury->GetInjuryState(), ESlayInjuryState::Healthy);
 Companion->Health->ApplyDamage(117.6f);
 TestEqual(TEXT("51 percent healthy"), Companion->Injury->GetInjuryState(), ESlayInjuryState::Healthy);
 Companion->Health->ApplyDamage(2.4f);
 TestEqual(TEXT("50 percent wounded"), Companion->Injury->GetInjuryState(), ESlayInjuryState::Wounded);
 Companion->Health->ApplyDamage(60.f);
 TestTrue(TEXT("25 percent half severity"), FMath::IsNearlyEqual(Companion->Injury->GetSeverity(), 0.5f));
 TestTrue(TEXT("Movement scales progressively"), FMath::IsNearlyEqual(Companion->GetCharacterMovement()->GetMaxSpeed(), BaseSpeed * 0.7f));
 TestTrue(TEXT("Attack speed scales"), FMath::IsNearlyEqual(Companion->Injury->GetAttackSpeedMultiplier(), 0.75f));
 TestTrue(TEXT("Dodge effectiveness scales"), FMath::IsNearlyEqual(Companion->Injury->GetDodgeEffectivenessMultiplier(), 0.625f));
 TestEqual(TEXT("Base stat untouched"), Companion->GetCharacterMovement()->MaxWalkSpeed, BaseSpeed);
 TestTrue(TEXT("Wounded still fights"), Companion->Melee->TryStartAttack());
 Fixture.Advance(0.65f);
 TestFalse(TEXT("Wounded recovery takes longer"), Companion->Melee->TryStartAttack());
 Fixture.Advance(0.2f);
 TestTrue(TEXT("Wounded attack eventually ready"), Companion->Melee->TryStartAttack());
 Companion->Health->Heal(100.f);
 TestEqual(TEXT("Healing above half restores healthy"), Companion->Injury->GetInjuryState(), ESlayInjuryState::Healthy);
 TestEqual(TEXT("Full performance restored"), Companion->GetCharacterMovement()->GetMaxSpeed(), BaseSpeed);
 Companion->Health->ApplyDamage(1000.f);
 TestFalse(TEXT("Companion never dead"), Companion->Health->IsDead());
 TestEqual(TEXT("Zero is incapacitated"), Companion->Injury->GetInjuryState(), ESlayInjuryState::Incapacitated);
 TestEqual(TEXT("No death event for companion"), Observer->Deaths, 0);
 TestEqual(TEXT("Collapse event once"), Observer->Collapses, 1);
 TestFalse(TEXT("All actions unavailable"), Companion->CanPerformActions());
 TestEqual(TEXT("No dodge effectiveness"), Companion->Injury->GetDodgeEffectivenessMultiplier(), 0.f);
 TestEqual(TEXT("No movement"), Companion->GetCharacterMovement()->GetMaxSpeed(), 0.f);
 TestTrue(TEXT("Incapacitation cancels attack"), Companion->Melee->GetAttackState() != ESlayAttackState::Attacking);
 TestEqual(TEXT("Ordinary healing cannot recover"), Companion->Health->Heal(100.f), 0.f);
 TestFalse(TEXT("Invalid recovery rejected"), Companion->Health->RecoverFromIncapacitation(-1.f));
 TestTrue(TEXT("Explicit recovery permitted"), Companion->Health->RecoverFromIncapacitation(0.25f));
 TestEqual(TEXT("Recovery restores HP fraction"), Companion->Health->GetCurrentHealth(), 60.f);
 TestTrue(TEXT("Recovered companion can act"), Companion->CanPerformActions());
 Companion->Health->ApplyDamage(1000.f);
 Companion->Health->bAllowHealingRecovery = true;
 TestEqual(TEXT("Opt-in healing recovery"), Companion->Health->Heal(180.f), 180.f);
 TestEqual(TEXT("Opt-in restores performance"), Companion->GetCharacterMovement()->GetMaxSpeed(), BaseSpeed);
 TestEqual(TEXT("Second collapse observed"), Observer->Collapses, 2);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayOcclusionTest, "SLAYCREW.Combat.WallsRangeAndCancellation",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlayOcclusionTest::RunTest(const FString& Parameters)
{
 SlayTests::FWorld Fixture;
 ASlayFighterCharacter* Player = Fixture.Spawn<ASlayFighterCharacter>();
 ASlayFighterCharacter* Target = Fixture.Spawn<ASlayFighterCharacter>(FVector(120, 0, 0));
 Target->Melee->CombatGroup = TEXT("Hostile");
 Player->DispatchBeginPlay();
 Target->DispatchBeginPlay();
 AActor* Wall = Fixture.Spawn<AActor>(FVector(60, 0, 0));
 UBoxComponent* Box = NewObject<UBoxComponent>(Wall);
 Wall->AddInstanceComponent(Box);
 Wall->SetRootComponent(Box);
 Box->SetBoxExtent(FVector(10, 100, 100));
 Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
 Box->SetCollisionObjectType(ECC_WorldStatic);
 Box->SetCollisionResponseToAllChannels(ECR_Block);
 Box->RegisterComponent();
 Wall->SetActorLocation(FVector(60, 0, 0));
 Fixture.Advance(0.02f);
 Player->Melee->TryStartAttack();
 TestEqual(TEXT("Wall prevents damage"), Target->Health->GetCurrentHealth(), 100.f);
 Player->Melee->CancelAttack();
 Wall->Destroy();
 Fixture.Advance(0.1f);
 TestEqual(TEXT("Canceled window cannot hit"), Target->Health->GetCurrentHealth(), 100.f);
 Fixture.Advance(0.6f);
 Target->SetActorLocation(FVector(500, 0, 0));
 Player->Melee->TryStartAttack();
 TestEqual(TEXT("Out of range misses"), Target->Health->GetCurrentHealth(), 100.f);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayEligibilityTest, "SLAYCREW.Combat.TargetEligibility",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlayEligibilityTest::RunTest(const FString& Parameters)
{
 SlayTests::FWorld Fixture;
 auto* Player = Fixture.Spawn<ASlayFighterCharacter>();
 auto* Ally = Fixture.Spawn<ASlayCompanionCharacter>(FVector(120, 0, 0));
 Player->DispatchBeginPlay();
 Ally->DispatchBeginPlay();
 Fixture.Advance(0.02f);
 TestFalse(TEXT("Party ally rejected"), Player->Melee->CanHitTarget(Ally));
 TestFalse(TEXT("Companion rejects protagonist"), Ally->Melee->CanHitTarget(Player));
 Player->Melee->TryStartAttack();
 TestEqual(TEXT("Ally receives no sweep damage"), Ally->Health->GetCurrentHealth(), 100.f);
 Fixture.Advance(0.7f);
 Player->Melee->bAllowFriendlyFire = true;
 TestTrue(TEXT("Friendly fire opt-in"), Player->Melee->CanHitTarget(Ally));
 Player->Melee->TryStartAttack();
 TestEqual(TEXT("Enabled friendly fire deals damage"), Ally->Health->GetCurrentHealth(), 75.f);
 Player->Melee->bAllowFriendlyFire = false;
 Ally->Melee->CombatGroup = TEXT("Hostile");
 TestTrue(TEXT("Different group eligible"), Player->Melee->CanHitTarget(Ally));
 Ally->Health->ApplyDamage(1000.f);
 TestFalse(TEXT("Incapacitated hostile rejected"), Player->Melee->CanHitTarget(Ally));
 Player->Melee->bAllowDepletedTargets = true;
 TestTrue(TEXT("Explicit depleted eligibility rule"), Player->Melee->CanHitTarget(Ally));
 Ally->Melee->CombatGroup = Player->Melee->CombatGroup;
 TestFalse(TEXT("Depletion opt-in does not bypass ally rule"), Player->Melee->CanHitTarget(Ally));
 TestFalse(TEXT("Self rejected"), Player->Melee->CanHitTarget(Player));
 TestFalse(TEXT("Null rejected"), Player->Melee->CanHitTarget(nullptr));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayMovementRecoveryTest, "SLAYCREW.Combat.MovementLockAndRecovery",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSlayMovementRecoveryTest::RunTest(const FString& Parameters)
{
 SlayTests::FWorld Fixture;
 auto* Companion = Fixture.Spawn<ASlayCompanionCharacter>();
 Companion->DispatchBeginPlay();
 auto* Movement = Companion->GetCharacterMovement();
 Movement->bRunPhysicsWithNoController = true;
 Movement->SetMovementMode(MOVE_Flying);
 Fixture.Advance(0.02f);
 Companion->Health->ApplyDamage(1000.f);
 const FVector Location = Companion->GetActorLocation();
 Movement->Velocity = FVector(500, 0, 0);
 Movement->AddImpulse(FVector(1000, 0, 0), true);
 Companion->AddMovementInput(FVector::ForwardVector, 1.f, true);
 Fixture.Advance(0.1f);
 TestEqual(TEXT("Depleted mode disabled"), Movement->MovementMode, MOVE_None);
 TestTrue(TEXT("Depleted position stays fixed"), Companion->GetActorLocation().Equals(Location));
 TestTrue(TEXT("Velocity cleared"), Movement->Velocity.IsZero());
 TestTrue(TEXT("Input consumed"), Companion->GetPendingMovementInputVector().IsZero());
 Companion->Health->RecoverFromIncapacitation(1.f);
 Fixture.Advance(0.02f);
 TestEqual(TEXT("Prior mode restored"), Movement->MovementMode, MOVE_Flying);
 Companion->AddMovementInput(FVector::ForwardVector, 1.f, true);
 Fixture.Advance(0.02f);
 TestTrue(TEXT("Recovered character actually moves"), Companion->GetActorLocation().X > Location.X);
 Movement->DisableMovement();
 Companion->Health->ApplyDamage(1000.f);
 Fixture.Advance(0.02f);
 Companion->Health->RecoverFromIncapacitation(1.f);
 Fixture.Advance(0.02f);
 TestEqual(TEXT("Preexisting movement disable preserved"), Movement->MovementMode, MOVE_None);
 return true;
}
#endif

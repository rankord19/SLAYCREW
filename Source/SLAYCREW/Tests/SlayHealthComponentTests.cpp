#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SlayHealthComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayHealthComponentTest, "SLAYCREW.Health.BoundsAndDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlayHealthComponentTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}
	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Test owner"), Owner))
	{
		World->DestroyWorld(false);
		return false;
	}
	USlayHealthComponent* Health = NewObject<USlayHealthComponent>(Owner);
	Owner->AddInstanceComponent(Health);
	Health->RegisterComponent();
	TestEqual(TEXT("Pre-play damage ignored"), Health->ApplyDamage(10.0f), 0.0f);
	Owner->DispatchBeginPlay();
	TestEqual(TEXT("Starts full"), Health->GetCurrentHealth(), Health->GetMaxHealth());
	TestEqual(TEXT("Negative damage ignored"), Health->ApplyDamage(-10.0f), 0.0f);
	TestEqual(TEXT("NaN damage ignored"), Health->ApplyDamage(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	TestEqual(TEXT("Infinite healing ignored"), Health->Heal(std::numeric_limits<float>::infinity()), 0.0f);
	UGameplayStatics::ApplyDamage(Owner, 25.0f, nullptr, nullptr, nullptr);
	TestEqual(TEXT("Engine damage routed"), Health->GetCurrentHealth(), 75.0f);
	TestEqual(TEXT("Healing clamps to capacity"), Health->Heal(std::numeric_limits<float>::max()), 25.0f);
	TestEqual(TEXT("Full health healing is a no-op"), Health->Heal(10.0f), 0.0f);
	TestEqual(TEXT("Lethal damage clamps"), Health->ApplyDamage(std::numeric_limits<float>::max()), 100.0f);
	TestTrue(TEXT("Dead at zero"), Health->IsDead());
	TestEqual(TEXT("Dead actors cannot heal"), Health->Heal(50.0f), 0.0f);
	TestEqual(TEXT("Repeated lethal damage is a no-op"), Health->ApplyDamage(50.0f), 0.0f);
	World->DestroyWorld(false);
	return true;
}

#endif

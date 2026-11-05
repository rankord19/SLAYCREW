#if WITH_DEV_AUTOMATION_TESTS

#include "Missions/SlayObjectiveSubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSlayObjectiveLifecycleTest, "SLAYCREW.Missions.ObjectiveLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSlayObjectiveLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("Test world"), World))
	{
		return false;
	}
	USlayObjectiveSubsystem* Objectives = World->GetSubsystem<USlayObjectiveSubsystem>();
	if (!TestNotNull(TEXT("Subsystem exists in game worlds"), Objectives))
	{
		World->DestroyWorld(false);
		return false;
	}

	TArray<TTuple<FName, ESlayObjectiveState, ESlayObjectiveState>> StateEvents;
	TArray<TTuple<FName, int32, int32>> ProgressEvents;
	Objectives->OnObjectiveStateChangedNative.AddLambda(
		[&StateEvents](FName Id, ESlayObjectiveState From, ESlayObjectiveState To)
		{
			StateEvents.Emplace(Id, From, To);
		});
	Objectives->OnObjectiveProgressChangedNative.AddLambda(
		[&ProgressEvents](FName Id, int32 Current, int32 Target)
		{
			ProgressEvents.Emplace(Id, Current, Target);
		});

	// Registration
	FSlayObjectiveDefinition Hunt;
	Hunt.ObjectiveId = TEXT("Hunt");
	Hunt.TargetProgress = 3;
	TestTrue(TEXT("Register succeeds"), Objectives->RegisterObjective(Hunt));
	TestFalse(TEXT("Duplicate ID rejected"), Objectives->RegisterObjective(Hunt));
	TestFalse(TEXT("None ID rejected"), Objectives->RegisterObjective(FSlayObjectiveDefinition()));
	TestEqual(TEXT("Starts inactive"), Objectives->GetObjectiveState(TEXT("Hunt")), ESlayObjectiveState::Inactive);
	TestFalse(TEXT("Inactive objectives ignore progress"), Objectives->AddObjectiveProgress(TEXT("Hunt")));
	TestFalse(TEXT("Inactive objectives cannot complete"), Objectives->CompleteObjective(TEXT("Hunt")));
	TestFalse(TEXT("Inactive objectives cannot fail"), Objectives->FailObjective(TEXT("Hunt")));
	TestFalse(TEXT("Unknown objectives ignored"), Objectives->ActivateObjective(TEXT("Missing")));
	TestEqual(TEXT("No events yet"), StateEvents.Num() + ProgressEvents.Num(), 0);

	// Activation and progression
	TestTrue(TEXT("Activate succeeds"), Objectives->ActivateObjective(TEXT("Hunt")));
	TestFalse(TEXT("Double activation rejected"), Objectives->ActivateObjective(TEXT("Hunt")));
	TestEqual(TEXT("Activation event"), StateEvents.Num(), 1);
	TestFalse(TEXT("Zero progress rejected"), Objectives->AddObjectiveProgress(TEXT("Hunt"), 0));
	TestFalse(TEXT("Negative progress rejected"), Objectives->AddObjectiveProgress(TEXT("Hunt"), -1));
	TestTrue(TEXT("Progress accepted"), Objectives->AddObjectiveProgress(TEXT("Hunt"), 1));
	TestEqual(TEXT("Still active below target"), Objectives->GetObjectiveState(TEXT("Hunt")), ESlayObjectiveState::Active);
	TestTrue(TEXT("Overshoot accepted"), Objectives->AddObjectiveProgress(TEXT("Hunt"), MAX_int32));

	FSlayObjectiveStatus Status;
	TestTrue(TEXT("Status found"), Objectives->GetObjectiveStatus(TEXT("Hunt"), Status));
	TestEqual(TEXT("Progress clamped to target"), Status.CurrentProgress, 3);
	TestEqual(TEXT("Completed on target"), Status.State, ESlayObjectiveState::Completed);
	TestEqual(TEXT("Two progress events"), ProgressEvents.Num(), 2);
	if (ProgressEvents.Num() == 2)
	{
		TestEqual(TEXT("Last progress value"), ProgressEvents[1].Get<1>(), 3);
	}
	TestEqual(TEXT("Completion event"), StateEvents.Num(), 2);
	if (StateEvents.Num() == 2)
	{
		TestEqual(TEXT("Completion from Active"), StateEvents[1].Get<1>(), ESlayObjectiveState::Active);
		TestEqual(TEXT("Completion to Completed"), StateEvents[1].Get<2>(), ESlayObjectiveState::Completed);
	}
	TestFalse(TEXT("Completed is terminal (fail)"), Objectives->FailObjective(TEXT("Hunt")));
	TestFalse(TEXT("Completed is terminal (progress)"), Objectives->AddObjectiveProgress(TEXT("Hunt")));

	// Failure, activation on register, manual completion
	FSlayObjectiveDefinition Escort;
	Escort.ObjectiveId = TEXT("Escort");
	Escort.bActivateOnRegister = true;
	TestTrue(TEXT("Auto-activated register"), Objectives->RegisterObjective(Escort));
	TestEqual(TEXT("Auto-activated"), Objectives->GetObjectiveState(TEXT("Escort")), ESlayObjectiveState::Active);
	TestTrue(TEXT("Fail succeeds"), Objectives->FailObjective(TEXT("Escort")));
	TestFalse(TEXT("Failed is terminal"), Objectives->CompleteObjective(TEXT("Escort")));

	FSlayObjectiveDefinition Gather;
	Gather.ObjectiveId = TEXT("Gather");
	Gather.TargetProgress = 0;
	Gather.bActivateOnRegister = true;
	TestTrue(TEXT("Register with invalid target"), Objectives->RegisterObjective(Gather));
	TestTrue(TEXT("Status found"), Objectives->GetObjectiveStatus(TEXT("Gather"), Status));
	TestEqual(TEXT("Target sanitized"), Status.Definition.TargetProgress, 1);
	TestTrue(TEXT("Manual complete"), Objectives->CompleteObjective(TEXT("Gather")));
	TestTrue(TEXT("Status found"), Objectives->GetObjectiveStatus(TEXT("Gather"), Status));
	TestEqual(TEXT("Manual complete fills progress"), Status.CurrentProgress, 1);

	TestEqual(TEXT("Completed query"), Objectives->GetObjectiveIdsInState(ESlayObjectiveState::Completed).Num(), 2);
	TestEqual(TEXT("Failed query"), Objectives->GetObjectiveIdsInState(ESlayObjectiveState::Failed).Num(), 1);
	TestEqual(TEXT("All objectives"), Objectives->GetAllObjectives().Num(), 3);

	// Re-entrancy: a listener failing the objective during progress must prevent completion.
	FSlayObjectiveDefinition Defend;
	Defend.ObjectiveId = TEXT("Defend");
	Defend.bActivateOnRegister = true;
	Objectives->RegisterObjective(Defend);
	Objectives->OnObjectiveProgressChangedNative.AddLambda([Objectives](FName Id, int32, int32)
	{
		if (Id == TEXT("Defend"))
		{
			Objectives->FailObjective(Id);
		}
	});
	TestTrue(TEXT("Re-entrant progress accepted"), Objectives->AddObjectiveProgress(TEXT("Defend")));
	TestEqual(TEXT("Listener failure wins"), Objectives->GetObjectiveState(TEXT("Defend")), ESlayObjectiveState::Failed);

	World->DestroyWorld(false);
	return true;
}

#endif

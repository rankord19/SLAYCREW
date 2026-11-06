// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AI/SLAYCREWEnemyBehaviorComponent.h"
#include "AI/SLAYCREWEnemyAIController.h"
#include "AI/SLAYCREWEnemyCharacter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FScopedTestWorld
	{
		UWorld* World = nullptr;

		FScopedTestWorld()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("SLAYCREWAITestWorld")));
			if (World && GEngine)
			{
				FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
				WorldContext.SetCurrentWorld(World);
				World->InitializeActorsForPlay(FURL());
				World->BeginPlay();
			}
		}

		~FScopedTestWorld()
		{
			if (World)
			{
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World);
				}
				World->DestroyWorld(false);
				World = nullptr;
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnemyBehaviorStateMachineTest,
	"SLAYCREW.AI.EnemyBehaviorStateMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSLAYCREWEnemyBehaviorStateMachineTest::RunTest(const FString& Parameters)
{
	USLAYCREWEnemyBehaviorComponent* BehaviorComp = NewObject<USLAYCREWEnemyBehaviorComponent>();
	TestNotNull(TEXT("Behavior component should be instantiated"), BehaviorComp);
	if (!BehaviorComp)
	{
		return false;
	}

	// Default initial state should be Idle
	TestEqual(
		TEXT("Initial state is Idle"),
		BehaviorComp->GetCurrentState(),
		ESLAYCREWEnemyBehaviorState::Idle);

	// Without patrol points or target, EvaluateBehavior remains in Idle
	BehaviorComp->EvaluateBehavior(5.0f);
	TestEqual(
		TEXT("Remains Idle when no patrol waypoints or target exist"),
		BehaviorComp->GetCurrentState(),
		ESLAYCREWEnemyBehaviorState::Idle);

	// Configure patrol waypoints and verify Idle -> Patrol transition after IdleDuration
	TArray<FVector> Waypoints;
	Waypoints.Add(FVector(500.0f, 0.0f, 0.0f));
	Waypoints.Add(FVector(1000.0f, 0.0f, 0.0f));
	BehaviorComp->SetPatrolWaypoints(Waypoints);
	BehaviorComp->SetBehaviorState(ESLAYCREWEnemyBehaviorState::Idle);

	BehaviorComp->EvaluateBehavior(BehaviorComp->BehaviorConfig.IdleDuration + 0.1f);
	TestEqual(
		TEXT("Transitions from Idle to Patrol after IdleDuration with valid waypoints"),
		BehaviorComp->GetCurrentState(),
		ESLAYCREWEnemyBehaviorState::Patrol);

	// Verify Patrol -> Chase transition when a valid target enters ChaseDetectionRange
	const ESLAYCREWEnemyBehaviorState DesiredChaseState = BehaviorComp->DetermineDesiredState(
		ESLAYCREWEnemyBehaviorState::Patrol,
		/*bHasValidTarget=*/true,
		/*DistanceToTarget=*/BehaviorComp->BehaviorConfig.ChaseDetectionRange * 0.8f,
		/*bHasPatrolPoints=*/true,
		/*InTimeInState=*/1.0f);
	TestEqual(
		TEXT("Transitions from Patrol to Chase when target is within ChaseDetectionRange"),
		DesiredChaseState,
		ESLAYCREWEnemyBehaviorState::Chase);

	// Verify Chase -> AttackIntent transition when target enters AttackIntentRange
	const ESLAYCREWEnemyBehaviorState DesiredAttackIntentState = BehaviorComp->DetermineDesiredState(
		ESLAYCREWEnemyBehaviorState::Chase,
		/*bHasValidTarget=*/true,
		/*DistanceToTarget=*/BehaviorComp->BehaviorConfig.AttackIntentRange * 0.9f,
		/*bHasPatrolPoints=*/true,
		/*InTimeInState=*/0.5f);
	TestEqual(
		TEXT("Transitions from Chase to AttackIntent when target is within AttackIntentRange"),
		DesiredAttackIntentState,
		ESLAYCREWEnemyBehaviorState::AttackIntent);

	// Verify AttackIntent hysteresis: stays in AttackIntent between AttackIntentRange and AttackIntentDisengageRange
	const float MidHysteresisDist =
		(BehaviorComp->BehaviorConfig.AttackIntentRange + BehaviorComp->BehaviorConfig.AttackIntentDisengageRange) * 0.5f;
	const ESLAYCREWEnemyBehaviorState HysteresisState = BehaviorComp->DetermineDesiredState(
		ESLAYCREWEnemyBehaviorState::AttackIntent,
		/*bHasValidTarget=*/true,
		/*DistanceToTarget=*/MidHysteresisDist,
		/*bHasPatrolPoints=*/true,
		/*InTimeInState=*/0.5f);
	TestEqual(
		TEXT("Remains in AttackIntent within disengage hysteresis band"),
		HysteresisState,
		ESLAYCREWEnemyBehaviorState::AttackIntent);

	// Verify AttackIntent -> Chase when target moves beyond AttackIntentDisengageRange
	const ESLAYCREWEnemyBehaviorState DisengageToChaseState = BehaviorComp->DetermineDesiredState(
		ESLAYCREWEnemyBehaviorState::AttackIntent,
		/*bHasValidTarget=*/true,
		/*DistanceToTarget=*/BehaviorComp->BehaviorConfig.AttackIntentDisengageRange + 50.0f,
		/*bHasPatrolPoints=*/true,
		/*InTimeInState=*/0.5f);
	TestEqual(
		TEXT("Transitions from AttackIntent back to Chase beyond AttackIntentDisengageRange"),
		DisengageToChaseState,
		ESLAYCREWEnemyBehaviorState::Chase);

	// Verify Chase -> Idle when target exceeds LoseTargetRange
	const ESLAYCREWEnemyBehaviorState LostTargetState = BehaviorComp->DetermineDesiredState(
		ESLAYCREWEnemyBehaviorState::Chase,
		/*bHasValidTarget=*/true,
		/*DistanceToTarget=*/BehaviorComp->BehaviorConfig.LoseTargetRange + 100.0f,
		/*bHasPatrolPoints=*/true,
		/*InTimeInState=*/2.0f);
	TestEqual(
		TEXT("Transitions from Chase to Idle when target exceeds LoseTargetRange"),
		LostTargetState,
		ESLAYCREWEnemyBehaviorState::Idle);

	// Verify waypoint advancement and looping
	TestEqual(TEXT("Initial waypoint index is 0"), BehaviorComp->GetCurrentPatrolWaypointIndex(), 0);
	TestTrue(TEXT("Advances to waypoint 1"), BehaviorComp->AdvancePatrolWaypoint());
	TestEqual(TEXT("Second waypoint index is 1"), BehaviorComp->GetCurrentPatrolWaypointIndex(), 1);
	TestTrue(TEXT("Loops back to waypoint 0"), BehaviorComp->AdvancePatrolWaypoint());
	TestEqual(TEXT("Looped waypoint index is 0"), BehaviorComp->GetCurrentPatrolWaypointIndex(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnemyPatrolNavigationEfficiencyTest,
	"SLAYCREW.AI.PatrolNavigationNoRedundantRequests",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSLAYCREWEnemyPatrolNavigationEfficiencyTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld ScopedWorld;
	TestNotNull(TEXT("Test world should be created"), ScopedWorld.World);
	if (!ScopedWorld.World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASLAYCREWEnemyCharacter* Enemy = ScopedWorld.World->SpawnActor<ASLAYCREWEnemyCharacter>(
		ASLAYCREWEnemyCharacter::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	ASLAYCREWEnemyAIController* AIController = ScopedWorld.World->SpawnActor<ASLAYCREWEnemyAIController>(
		ASLAYCREWEnemyAIController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	TestNotNull(TEXT("Enemy character spawned"), Enemy);
	TestNotNull(TEXT("Enemy AI controller spawned"), AIController);
	if (!Enemy || !AIController)
	{
		return false;
	}

	AIController->bAutoDetectPlayerTargets = false;
	AIController->Possess(Enemy);

	USLAYCREWEnemyBehaviorComponent* BehaviorComp = Enemy->GetEnemyBehaviorComponent();
	TestNotNull(TEXT("Behavior component exists on enemy"), BehaviorComp);
	if (!BehaviorComp)
	{
		return false;
	}

	BehaviorComp->BehaviorConfig.bPauseAtPatrolWaypoints = false;
	BehaviorComp->BehaviorConfig.bLoopPatrol = true;
	BehaviorComp->BehaviorConfig.PatrolAcceptanceRadius = 100.0f;

	TArray<FVector> Waypoints;
	Waypoints.Add(FVector(600.0f, 0.0f, 0.0f));
	Waypoints.Add(FVector(1200.0f, 0.0f, 0.0f));
	BehaviorComp->SetPatrolWaypoints(Waypoints);

	AIController->ResetPatrolMoveRequestCount();

	// 1. Entering Patrol should issue exactly 1 MoveToLocation request for Waypoint 0
	BehaviorComp->SetBehaviorState(ESLAYCREWEnemyBehaviorState::Patrol);
	TestEqual(
		TEXT("Entering Patrol issues exactly 1 MoveToLocation request"),
		AIController->GetPatrolMoveRequestCount(),
		1);

	// 2. Ticking the AIController repeatedly while en route to Waypoint 0 must NOT issue redundant requests
	for (int32 TickIdx = 0; TickIdx < 20; ++TickIdx)
	{
		BehaviorComp->EvaluateBehavior(0.05f);
		AIController->Tick(0.05f);
	}
	TestEqual(
		TEXT("20 patrol ticks at same waypoint issue 0 redundant MoveToLocation requests"),
		AIController->GetPatrolMoveRequestCount(),
		1);

	// 3. Reaching Waypoint 0 advances to Waypoint 1 (continuous patrol without pause) and issues 1 new request
	Enemy->SetActorLocation(Waypoints[0]);
	BehaviorComp->EvaluateBehavior(0.05f);
	AIController->Tick(0.05f);

	TestEqual(TEXT("Remains in Patrol when bPauseAtPatrolWaypoints is false"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Patrol);
	TestEqual(TEXT("Waypoint index advanced to 1"), BehaviorComp->GetCurrentPatrolWaypointIndex(), 1);
	TestEqual(TEXT("Advancing to Waypoint 1 issues exactly 1 new MoveToLocation request"), AIController->GetPatrolMoveRequestCount(), 2);

	// Additional ticks while traveling to Waypoint 1 issue 0 additional requests
	for (int32 TickIdx = 0; TickIdx < 10; ++TickIdx)
	{
		BehaviorComp->EvaluateBehavior(0.05f);
		AIController->Tick(0.05f);
	}
	TestEqual(TEXT("Subsequent ticks toward Waypoint 1 do not re-issue MoveToLocation"), AIController->GetPatrolMoveRequestCount(), 2);

	// 4. Reaching Waypoint 1 loops back to Waypoint 0 and issues 1 new request
	Enemy->SetActorLocation(Waypoints[1]);
	BehaviorComp->EvaluateBehavior(0.05f);
	AIController->Tick(0.05f);

	TestEqual(TEXT("Looped back to Waypoint 0"), BehaviorComp->GetCurrentPatrolWaypointIndex(), 0);
	TestEqual(TEXT("Looping back to Waypoint 0 issues 1 new MoveToLocation request"), AIController->GetPatrolMoveRequestCount(), 3);

	// 5. Simulated movement interruption triggers a single recovery MoveToLocation after PatrolMoveRetryInterval
	AIController->PatrolMoveRetryInterval = 0.25f;
	AIController->NotifyPatrolMovementInterrupted();
	AIController->Tick(0.10f);
	TestEqual(TEXT("No retry before PatrolMoveRetryInterval expires"), AIController->GetPatrolMoveRequestCount(), 3);

	AIController->Tick(0.20f);
	TestEqual(TEXT("Recovery MoveToLocation issued once retry cooldown expires"), AIController->GetPatrolMoveRequestCount(), 4);

	// 6. Verify pause-at-waypoint behavior and non-looping termination
	BehaviorComp->BehaviorConfig.bPauseAtPatrolWaypoints = true;
	BehaviorComp->BehaviorConfig.IdleDuration = 1.0f;
	BehaviorComp->BehaviorConfig.bLoopPatrol = false;

	Enemy->SetActorLocation(Waypoints[0]);
	BehaviorComp->EvaluateBehavior(0.05f);
	TestEqual(TEXT("Pauses in Idle upon reaching Waypoint 0 when bPauseAtPatrolWaypoints is true"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Idle);
	TestEqual(TEXT("Next waypoint index is 1 while paused"), BehaviorComp->GetCurrentPatrolWaypointIndex(), 1);

	BehaviorComp->EvaluateBehavior(1.1f);
	TestEqual(TEXT("Resumes Patrol after IdleDuration expires"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Patrol);
	TestEqual(TEXT("Resuming Patrol issues 1 MoveToLocation for Waypoint 1"), AIController->GetPatrolMoveRequestCount(), 5);

	// Reaching final Waypoint 1 with bLoopPatrol = false ends patrol and stays Idle
	Enemy->SetActorLocation(Waypoints[1]);
	BehaviorComp->EvaluateBehavior(0.05f);
	TestEqual(TEXT("Transitions to Idle at end of non-looping patrol"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Idle);
	TestFalse(TEXT("No active patrol waypoints remain after non-looping completion"), BehaviorComp->HasActivePatrolWaypoints());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSLAYCREWEnemyTargetEligibilityAndReacquisitionTest,
	"SLAYCREW.AI.TargetEligibilityLossAndReacquisition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSLAYCREWEnemyTargetEligibilityAndReacquisitionTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld ScopedWorld;
	TestNotNull(TEXT("Test world should be created"), ScopedWorld.World);
	if (!ScopedWorld.World)
	{
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ASLAYCREWEnemyCharacter* Enemy = ScopedWorld.World->SpawnActor<ASLAYCREWEnemyCharacter>(
		ASLAYCREWEnemyCharacter::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);
	ASLAYCREWEnemyAIController* AIController = ScopedWorld.World->SpawnActor<ASLAYCREWEnemyAIController>(
		ASLAYCREWEnemyAIController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParams);

	// Spawn a Protagonist actor and a Companion actor
	AActor* Protagonist = ScopedWorld.World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FVector(400.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParams);
	USceneComponent* ProtagonistRoot = NewObject<USceneComponent>(Protagonist);
	Protagonist->SetRootComponent(ProtagonistRoot);
	ProtagonistRoot->RegisterComponent();
	Protagonist->SetActorLocation(FVector(400.0f, 0.0f, 0.0f));

	AActor* Companion = ScopedWorld.World->SpawnActor<AActor>(
		AActor::StaticClass(),
		FVector(600.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator,
		SpawnParams);
	USceneComponent* CompanionRoot = NewObject<USceneComponent>(Companion);
	Companion->SetRootComponent(CompanionRoot);
	CompanionRoot->RegisterComponent();
	Companion->SetActorLocation(FVector(600.0f, 0.0f, 0.0f));

	AIController->Possess(Enemy);
	AIController->RegisterCandidateTarget(Protagonist);
	AIController->RegisterCandidateTarget(Companion);

	USLAYCREWEnemyBehaviorComponent* BehaviorComp = Enemy->GetEnemyBehaviorComponent();
	TestNotNull(TEXT("Behavior component exists on enemy"), BehaviorComp);
	if (!BehaviorComp)
	{
		return false;
	}

	// 1. Auto-acquires closer eligible target (Protagonist at 400cm) and enters Chase
	AIController->Tick(0.1f);
	BehaviorComp->EvaluateBehavior(0.1f);
	TestEqual(TEXT("Acquires closer eligible Protagonist"), BehaviorComp->GetTargetActor(), Protagonist);
	TestEqual(TEXT("Transitions to Chase for Protagonist"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Chase);

	// 2. Target loss by distance: moving Protagonist beyond LoseTargetRange drops target; AI reacquires Companion (at 600cm)
	Protagonist->SetActorLocation(FVector(BehaviorComp->BehaviorConfig.LoseTargetRange + 500.0f, 0.0f, 0.0f));
	BehaviorComp->EvaluateBehavior(0.1f);
	TestNull(TEXT("Protagonist is dropped when exceeding LoseTargetRange"), BehaviorComp->GetTargetActor());
	TestEqual(TEXT("Enemy returns to Idle immediately upon losing target"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Idle);

	AIController->Tick(0.1f);
	TestEqual(TEXT("Enemy automatically reacquires in-range Companion"), BehaviorComp->GetTargetActor(), Companion);
	TestEqual(TEXT("Enemy enters Chase toward Companion"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Chase);

	// 3. Companion becomes incapacitated at 0 HP (remains alive in world, tagged "Incapacitated")
	Companion->Tags.Add(FName(TEXT("Incapacitated")));
	TestFalse(TEXT("Incapacitated Companion is not combat-eligible"), BehaviorComp->IsTargetEligible(Companion));

	AIController->Tick(0.1f);
	BehaviorComp->EvaluateBehavior(0.1f);
	TestNull(TEXT("Enemy drops incapacitated Companion immediately"), BehaviorComp->GetTargetActor());
	TestEqual(TEXT("Enemy returns to Idle when Companion is incapacitated and Protagonist is out of range"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Idle);

	// 4. Bring Protagonist into AttackIntentRange -> Enemy acquires Protagonist and enters AttackIntent
	Protagonist->SetActorLocation(FVector(150.0f, 0.0f, 0.0f));
	AIController->Tick(0.1f);
	BehaviorComp->EvaluateBehavior(0.1f);
	TestEqual(TEXT("Enemy acquires Protagonist in AttackIntentRange"), BehaviorComp->GetTargetActor(), Protagonist);
	TestEqual(TEXT("Enemy transitions to AttackIntent"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::AttackIntent);

	// 5. Protagonist dies at 0 HP (tagged "Dead") -> Enemy immediately stops attacking and clears target
	Protagonist->Tags.Add(FName(TEXT("Dead")));
	TestFalse(TEXT("Dead Protagonist is not combat-eligible"), BehaviorComp->IsTargetEligible(Protagonist));
	TestFalse(TEXT("SetTargetActor rejects dead Protagonist"), (BehaviorComp->SetTargetActor(Protagonist), BehaviorComp->HasValidCombatTarget()));

	AIController->Tick(0.1f);
	BehaviorComp->EvaluateBehavior(0.1f);
	TestNull(TEXT("Dead Protagonist is cleared from CurrentTarget"), BehaviorComp->GetTargetActor());
	TestEqual(TEXT("Enemy exits AttackIntent to Idle when Protagonist dies"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Idle);

	// 6. Companion recovers from incapacitation -> Enemy reacquires revived Companion
	Companion->Tags.Remove(FName(TEXT("Incapacitated")));
	TestTrue(TEXT("Revived Companion is once again combat-eligible"), BehaviorComp->IsTargetEligible(Companion));

	AIController->Tick(0.1f);
	BehaviorComp->EvaluateBehavior(0.1f);
	TestEqual(TEXT("Enemy reacquires revived Companion"), BehaviorComp->GetTargetActor(), Companion);
	TestEqual(TEXT("Enemy resumes Chase toward revived Companion"), BehaviorComp->GetCurrentState(), ESLAYCREWEnemyBehaviorState::Chase);

	// 7. CustomTargetEligibilityPredicate integration hook test (simulating external Health/Injury state)
	bool bExternalCompanionCombatCapable = false;
	BehaviorComp->CustomTargetEligibilityPredicate.BindLambda([&](const AActor* Candidate) -> bool
	{
		return (Candidate == Companion) ? bExternalCompanionCombatCapable : true;
	});

	TestFalse(TEXT("CustomTargetEligibilityPredicate disqualifies Companion when returning false"), BehaviorComp->IsTargetEligible(Companion));
	AIController->Tick(0.1f);
	BehaviorComp->EvaluateBehavior(0.1f);
	TestNull(TEXT("Companion dropped when CustomTargetEligibilityPredicate returns false"), BehaviorComp->GetTargetActor());

	bExternalCompanionCombatCapable = true;
	AIController->Tick(0.1f);
	TestEqual(TEXT("Companion reacquired when CustomTargetEligibilityPredicate returns true"), BehaviorComp->GetTargetActor(), Companion);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

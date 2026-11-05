// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "AI/SLAYCREWEnemyBehaviorComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

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

#endif // WITH_DEV_AUTOMATION_TESTS

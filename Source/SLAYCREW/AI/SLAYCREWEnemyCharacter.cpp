// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/SLAYCREWEnemyCharacter.h"
#include "AI/SLAYCREWEnemyAIController.h"
#include "AI/SLAYCREWEnemyBehaviorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ASLAYCREWEnemyCharacter::ASLAYCREWEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	AIControllerClass = ASLAYCREWEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	EnemyBehaviorComponent = CreateDefaultSubobject<USLAYCREWEnemyBehaviorComponent>(TEXT("EnemyBehaviorComponent"));

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->MaxWalkSpeed = PatrolWalkSpeed;
	}
}

void ASLAYCREWEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (EnemyBehaviorComponent)
	{
		EnemyBehaviorComponent->OnBehaviorStateChanged.AddDynamic(
			this,
			&ASLAYCREWEnemyCharacter::HandleBehaviorStateChanged);
		EnemyBehaviorComponent->OnAttackIntent.AddDynamic(
			this,
			&ASLAYCREWEnemyCharacter::HandleAttackIntent);

		for (const AActor* WaypointActor : PatrolWaypointActors)
		{
			if (IsValid(WaypointActor))
			{
				EnemyBehaviorComponent->AddPatrolWaypoint(WaypointActor->GetActorLocation());
			}
		}

		UpdateMovementSpeedForState(EnemyBehaviorComponent->GetCurrentState());
	}
}

void ASLAYCREWEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EnemyBehaviorComponent)
	{
		EnemyBehaviorComponent->OnBehaviorStateChanged.RemoveDynamic(
			this,
			&ASLAYCREWEnemyCharacter::HandleBehaviorStateChanged);
		EnemyBehaviorComponent->OnAttackIntent.RemoveDynamic(
			this,
			&ASLAYCREWEnemyCharacter::HandleAttackIntent);
	}

	Super::EndPlay(EndPlayReason);
}

void ASLAYCREWEnemyCharacter::HandleBehaviorStateChanged(
	ESLAYCREWEnemyBehaviorState PreviousState,
	ESLAYCREWEnemyBehaviorState NewState)
{
	UpdateMovementSpeedForState(NewState);
	ReceiveBehaviorStateChanged(PreviousState, NewState);
}

void ASLAYCREWEnemyCharacter::HandleAttackIntent(AActor* TargetActor)
{
	ReceiveAttackIntent(TargetActor);
}

void ASLAYCREWEnemyCharacter::UpdateMovementSpeedForState(ESLAYCREWEnemyBehaviorState State)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	switch (State)
	{
	case ESLAYCREWEnemyBehaviorState::Chase:
	case ESLAYCREWEnemyBehaviorState::AttackIntent:
		MoveComp->MaxWalkSpeed = ChaseWalkSpeed;
		break;

	case ESLAYCREWEnemyBehaviorState::Idle:
	case ESLAYCREWEnemyBehaviorState::Patrol:
	default:
		MoveComp->MaxWalkSpeed = PatrolWalkSpeed;
		break;
	}
}

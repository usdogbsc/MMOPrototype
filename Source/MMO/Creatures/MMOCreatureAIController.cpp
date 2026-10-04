// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMOCreatureAIController.h"
#include "Creatures/MMOCreature.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MMO.h"

AMMOCreatureAIController::AMMOCreatureAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMMOCreatureAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AMMOCreature* Creature = GetCreature();
	if (!Creature)
	{
		return;
	}

	// stunned: stand still and do nothing until it wears off
	if (Creature->IsStunned())
	{
		StopMoving();
		return;
	}

	switch (State)
	{
	case EMMOCreatureAIState::Idle:			TickIdle(Creature); break;
	case EMMOCreatureAIState::Chasing:		TickChasing(Creature); break;
	case EMMOCreatureAIState::Attacking:	TickAttacking(Creature, DeltaSeconds); break;
	case EMMOCreatureAIState::Returning:	TickReturning(Creature); break;
	case EMMOCreatureAIState::Dead:			break;
	}
}

void AMMOCreatureAIController::NotifyDamagedBy(AActor* DamageInstigator)
{
	if (State == EMMOCreatureAIState::Idle && IsValidThreat(DamageInstigator))
	{
		ThreatTarget = DamageInstigator;
		SetState(EMMOCreatureAIState::Chasing);
	}
}

void AMMOCreatureAIController::NotifyPawnDied()
{
	ThreatTarget.Reset();
	StopMoving();
	SetState(EMMOCreatureAIState::Dead);
}

void AMMOCreatureAIController::NotifyPawnRespawned()
{
	bWandering = false;
	NextWanderTime = 0.0;
	ThreatTarget.Reset();
	StopMoving();
	SetState(EMMOCreatureAIState::Idle);
}

bool AMMOCreatureAIController::IsUsingNavigation() const
{
	return GetPathFollowingComponent() && GetPathFollowingComponent()->GetStatus() == EPathFollowingStatus::Moving;
}

void AMMOCreatureAIController::SetState(EMMOCreatureAIState NewState)
{
	if (State == NewState)
	{
		return;
	}

	UE_LOG(LogMMO, Verbose, TEXT("%s: %s -> %s"), *GetNameSafe(GetPawn()),
		*UEnum::GetValueAsString(State), *UEnum::GetValueAsString(NewState));

	const EMMOCreatureAIState OldState = State;
	State = NewState;

	if (AMMOCreature* Creature = GetCreature())
	{
		Creature->SetEvading(State == EMMOCreatureAIState::Returning);

		if (OldState == EMMOCreatureAIState::Idle && State == EMMOCreatureAIState::Chasing)
		{
			Creature->OnAggro(ThreatTarget.Get());
		}
	}
}

AMMOCreature* AMMOCreatureAIController::GetCreature() const
{
	return Cast<AMMOCreature>(GetPawn());
}

bool AMMOCreatureAIController::IsValidThreat(const AActor* Target)
{
	if (!IsValid(Target))
	{
		return false;
	}

	const UMMOHealthComponent* Health = Target->FindComponentByClass<UMMOHealthComponent>();
	return Health && !Health->IsDead();
}

AActor* AMMOCreatureAIController::ChooseThreat(const AMMOCreature* Creature) const
{
	// single-player prototype: the only potential threat is the local player
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (IsValidThreat(Player) && FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation()) <= Creature->AggroRange)
	{
		return Player;
	}
	return nullptr;
}

void AMMOCreatureAIController::TickIdle(AMMOCreature* Creature)
{
	if (AActor* Threat = ChooseThreat(Creature))
	{
		bWandering = false;
		ThreatTarget = Threat;
		SetState(EMMOCreatureAIState::Chasing);
		return;
	}

	TickWander(Creature);
}

void AMMOCreatureAIController::ScheduleNextWander(const AMMOCreature* Creature)
{
	NextWanderTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(Creature->WanderPause.X, FMath::Max(Creature->WanderPause.X, Creature->WanderPause.Y));
}

void AMMOCreatureAIController::TickWander(AMMOCreature* Creature)
{
	if (Creature->WanderRadius <= 0.0f)
	{
		return;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (bWandering)
	{
		// arrived (or the move ended): pause and look around before the next stroll
		if (!IsUsingNavigation())
		{
			bWandering = false;
			ScheduleNextWander(Creature);
		}
		return;
	}

	if (NextWanderTime == 0.0)
	{
		ScheduleNextWander(Creature);
		return;
	}
	if (Now < NextWanderTime)
	{
		return;
	}

	// a short walk to a reachable spot near home
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Destination;
	if (NavSys && NavSys->GetRandomReachablePointInRadius(Creature->GetSpawnTransform().GetLocation(), Creature->WanderRadius, Destination))
	{
		Creature->GetCharacterMovement()->MaxWalkSpeed = Creature->WanderSpeed;
		if (MoveToLocation(Destination.Location, 40.0f, false, true, false, false, nullptr, false) != EPathFollowingRequestResult::Failed)
		{
			bWandering = true;
			return;
		}
	}
	ScheduleNextWander(Creature);
}

void AMMOCreatureAIController::TickChasing(AMMOCreature* Creature)
{
	if (ShouldReturn(Creature))
	{
		SetState(EMMOCreatureAIState::Returning);
		return;
	}

	AActor* Target = ThreatTarget.Get();
	if (UMMOCombatComponent::GetEdgeDistance(Creature, Target) <= Creature->AttackRange)
	{
		StopMoving();
		SetState(EMMOCreatureAIState::Attacking);
		return;
	}

	MoveTowardGoal(Creature, Target);
}

void AMMOCreatureAIController::TickAttacking(AMMOCreature* Creature, float DeltaSeconds)
{
	if (ShouldReturn(Creature))
	{
		SetState(EMMOCreatureAIState::Returning);
		return;
	}

	// commit to an attack in progress: no turning or moving until the bite resolves
	if (Creature->IsAttacking())
	{
		return;
	}

	AActor* Target = ThreatTarget.Get();
	const float Distance = UMMOCombatComponent::GetEdgeDistance(Creature, Target);
	if (Distance > Creature->AttackRange + AttackRangeHysteresis)
	{
		SetState(EMMOCreatureAIState::Chasing);
		return;
	}

	// hold position and turn to face the target
	FVector ToTarget = Target->GetActorLocation() - Creature->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.IsNearlyZero())
	{
		Creature->SetActorRotation(FMath::RInterpTo(Creature->GetActorRotation(), ToTarget.Rotation(), DeltaSeconds, 10.0f));
	}

	// inside the hysteresis band: close the small gap rather than attack from too far
	if (Distance > Creature->AttackRange)
	{
		SteerToward(Creature, Target->GetActorLocation());
		return;
	}

	if (Creature->CanAttack(Target))
	{
		Creature->PerformAttack(Target);
	}
}

void AMMOCreatureAIController::TickReturning(AMMOCreature* Creature)
{
	const FTransform& Home = Creature->GetSpawnTransform();
	if (FVector::Dist2D(Creature->GetActorLocation(), Home.GetLocation()) <= HomeAcceptanceRadius)
	{
		// classic MMO reset: back home at full health
		StopMoving();
		Creature->SetActorRotation(Home.Rotator());
		Creature->GetHealth()->ResetHealth();
		Creature->NotifyCombatReset();
		ThreatTarget.Reset();
		bWandering = false;
		NextWanderTime = 0.0;
		SetState(EMMOCreatureAIState::Idle);
		return;
	}

	MoveTowardGoal(Creature, nullptr);
}

bool AMMOCreatureAIController::ShouldReturn(const AMMOCreature* Creature) const
{
	if (!IsValidThreat(ThreatTarget.Get()))
	{
		return true;
	}

	// pulled too far from home, or the target has fled well beyond this creature's territory
	// (the second rule also frees creatures that can't reach a target, e.g. a boss in a narrow tunnel)
	const FVector Home = Creature->GetSpawnTransform().GetLocation();
	return FVector::Dist2D(Creature->GetActorLocation(), Home) > Creature->LeashRange
		|| FVector::Dist2D(ThreatTarget->GetActorLocation(), Home) > Creature->LeashRange + Creature->AggroRange;
}

void AMMOCreatureAIController::MoveTowardGoal(AMMOCreature* Creature, AActor* Goal)
{
	const bool bGoalIsHome = Goal == nullptr;
	const FVector Destination = bGoalIsHome ? Creature->GetSpawnTransform().GetLocation() : Goal->GetActorLocation();

	// recovering from a failed path request: steer directly for a moment
	if (GetWorld()->GetTimeSeconds() < DirectSteerUntil)
	{
		SteerToward(Creature, Destination);
		return;
	}

	// already following a path to this goal (moving goals are tracked by the path following component)
	if (IsUsingNavigation() && bMovingHome == bGoalIsHome && MoveGoalActor.Get() == Goal)
	{
		return;
	}

	EPathFollowingRequestResult::Type Result;
	if (bGoalIsHome)
	{
		Result = MoveToLocation(Destination, HomeAcceptanceRadius * 0.5f, false, true, true, false, nullptr, true);
	}
	else
	{
		Result = MoveToActor(Goal, Creature->AttackRange * 0.5f, true, true, false, nullptr, true);
	}

	if (Result == EPathFollowingRequestResult::Failed)
	{
		UE_LOG(LogMMO, Verbose, TEXT("%s: path request failed, steering directly"), *GetNameSafe(Creature));
		DirectSteerUntil = GetWorld()->GetTimeSeconds() + NavigationRetryDelay;
		MoveGoalActor.Reset();
		SteerToward(Creature, Destination);
		return;
	}

	MoveGoalActor = Goal;
	bMovingHome = bGoalIsHome;
}

void AMMOCreatureAIController::StopMoving()
{
	StopMovement();
	MoveGoalActor.Reset();
	bMovingHome = false;
	DirectSteerUntil = 0.0;
}

void AMMOCreatureAIController::SteerToward(AMMOCreature* Creature, const FVector& Destination)
{
	const FVector Direction = (Destination - Creature->GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		Creature->AddMovementInput(Direction, 1.0f);
	}
}

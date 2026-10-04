// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMOCreatureAIController.h"
#include "Creatures/MMOCreature.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Kismet/GameplayStatics.h"
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
	SetState(EMMOCreatureAIState::Dead);
}

void AMMOCreatureAIController::NotifyPawnRespawned()
{
	ThreatTarget.Reset();
	SetState(EMMOCreatureAIState::Idle);
}

void AMMOCreatureAIController::SetState(EMMOCreatureAIState NewState)
{
	if (State == NewState)
	{
		return;
	}

	UE_LOG(LogMMO, Verbose, TEXT("%s: %s -> %s"), *GetNameSafe(GetPawn()),
		*UEnum::GetValueAsString(State), *UEnum::GetValueAsString(NewState));

	State = NewState;

	if (AMMOCreature* Creature = GetCreature())
	{
		Creature->SetEvading(State == EMMOCreatureAIState::Returning);
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

void AMMOCreatureAIController::TickIdle(AMMOCreature* Creature)
{
	// single-player prototype: the only potential threat is the local player
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsValidThreat(Player))
	{
		return;
	}

	if (FVector::Dist(Creature->GetActorLocation(), Player->GetActorLocation()) <= Creature->AggroRange)
	{
		ThreatTarget = Player;
		SetState(EMMOCreatureAIState::Chasing);
	}
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
		SetState(EMMOCreatureAIState::Attacking);
		return;
	}

	SteerToward(Creature, Target->GetActorLocation());
}

void AMMOCreatureAIController::TickAttacking(AMMOCreature* Creature, float DeltaSeconds)
{
	if (ShouldReturn(Creature))
	{
		SetState(EMMOCreatureAIState::Returning);
		return;
	}

	AActor* Target = ThreatTarget.Get();
	if (UMMOCombatComponent::GetEdgeDistance(Creature, Target) > Creature->AttackRange + AttackRangeHysteresis)
	{
		SetState(EMMOCreatureAIState::Chasing);
		return;
	}

	// hold position and turn to face the target
	FVector ToTarget = Target->GetActorLocation() - Creature->GetActorLocation();
	ToTarget.Z = 0.0f;
	if (!ToTarget.IsNearlyZero())
	{
		const FRotator Desired = ToTarget.Rotation();
		Creature->SetActorRotation(FMath::RInterpTo(Creature->GetActorRotation(), Desired, DeltaSeconds, 10.0f));
	}

	// the target may have closed in just inside the hysteresis band: nudge forward rather than attack from too far
	if (UMMOCombatComponent::GetEdgeDistance(Creature, Target) > Creature->AttackRange)
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
		Creature->SetActorRotation(Home.Rotator());
		Creature->GetHealth()->ResetHealth();
		ThreatTarget.Reset();
		SetState(EMMOCreatureAIState::Idle);
		return;
	}

	SteerToward(Creature, Home.GetLocation());
}

bool AMMOCreatureAIController::ShouldReturn(const AMMOCreature* Creature) const
{
	if (!IsValidThreat(ThreatTarget.Get()))
	{
		return true;
	}

	return FVector::Dist2D(Creature->GetActorLocation(), Creature->GetSpawnTransform().GetLocation()) > Creature->LeashRange;
}

void AMMOCreatureAIController::SteerToward(AMMOCreature* Creature, const FVector& Destination)
{
	const FVector Direction = (Destination - Creature->GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		Creature->AddMovementInput(Direction, 1.0f);
	}
}

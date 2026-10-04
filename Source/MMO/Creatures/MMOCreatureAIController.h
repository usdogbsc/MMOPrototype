// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "MMOCreatureAIController.generated.h"

class AMMOCreature;

/** Creature behaviour states */
UENUM(BlueprintType)
enum class EMMOCreatureAIState : uint8
{
	Idle,
	Chasing,
	Attacking,
	Returning,
	Dead
};

/**
 *  Simple state machine for hostile creatures:
 *  Idle -> (player in aggro range or attacked) -> Chasing <-> Attacking -> (leash exceeded / target dead) -> Returning -> Idle
 *  Moves with NavMesh pathfinding. If a path request fails (no NavMesh, off-mesh), it steers directly for a moment and retries.
 *  Each creature picks its own hostile target (the player); a threat table can replace ChooseThreat later.
 */
UCLASS()
class AMMOCreatureAIController : public AAIController
{
	GENERATED_BODY()

public:

	AMMOCreatureAIController();

	virtual void Tick(float DeltaSeconds) override;

	/** Called by the creature when it takes damage. Pulls an idle creature into combat */
	void NotifyDamagedBy(AActor* DamageInstigator);

	void NotifyPawnDied();

	void NotifyPawnRespawned();

	UFUNCTION(BlueprintPure, Category="AI")
	EMMOCreatureAIState GetAIState() const { return State; }

	UFUNCTION(BlueprintPure, Category="AI")
	AActor* GetThreatTarget() const { return ThreatTarget.Get(); }

	/** True while following a NavMesh path (false while steering directly or standing still) */
	UFUNCTION(BlueprintPure, Category="AI")
	bool IsUsingNavigation() const;

protected:

	/** Extra distance beyond attack range before the creature resumes chasing, to prevent jitter */
	UPROPERTY(EditAnywhere, Category="AI", meta=(Units="cm"))
	float AttackRangeHysteresis = 40.0f;

	/** Distance from spawn at which a returning creature counts as home */
	UPROPERTY(EditAnywhere, Category="AI", meta=(Units="cm"))
	float HomeAcceptanceRadius = 60.0f;

	/** Seconds to steer directly after a failed path request before retrying navigation */
	UPROPERTY(EditAnywhere, Category="AI", meta=(Units="s"))
	float NavigationRetryDelay = 1.0f;

	UPROPERTY(VisibleInstanceOnly, Category="AI")
	EMMOCreatureAIState State = EMMOCreatureAIState::Idle;

	TWeakObjectPtr<AActor> ThreatTarget;

	/** What the current path request is heading for */
	TWeakObjectPtr<AActor> MoveGoalActor;
	bool bMovingHome = false;

	/** World time until which we steer directly instead of pathing */
	double DirectSteerUntil = 0.0;

	void SetState(EMMOCreatureAIState NewState);

	AMMOCreature* GetCreature() const;

	/** True if Target exists and is alive */
	static bool IsValidThreat(const AActor* Target);

	/** Picks a hostile target among nearby candidates (single player for now) */
	AActor* ChooseThreat(const AMMOCreature* Creature) const;

	void TickIdle(AMMOCreature* Creature);
	void TickChasing(AMMOCreature* Creature);
	void TickAttacking(AMMOCreature* Creature, float DeltaSeconds);
	void TickReturning(AMMOCreature* Creature);

	/** True if the creature has strayed too far from home or lost its target */
	bool ShouldReturn(const AMMOCreature* Creature) const;

	/** Path toward an actor, or toward home if Goal is null */
	void MoveTowardGoal(AMMOCreature* Creature, AActor* Goal);

	void StopMoving();

	void SteerToward(AMMOCreature* Creature, const FVector& Destination);
};

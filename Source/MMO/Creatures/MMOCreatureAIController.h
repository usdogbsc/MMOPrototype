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
 *  Moves by steering directly toward its goal, so it works without a NavMesh.
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

protected:

	/** Extra distance beyond attack range before the creature resumes chasing, to prevent jitter */
	UPROPERTY(EditAnywhere, Category="AI", meta=(Units="cm"))
	float AttackRangeHysteresis = 40.0f;

	/** Distance from spawn at which a returning creature counts as home */
	UPROPERTY(EditAnywhere, Category="AI", meta=(Units="cm"))
	float HomeAcceptanceRadius = 60.0f;

	UPROPERTY(VisibleInstanceOnly, Category="AI")
	EMMOCreatureAIState State = EMMOCreatureAIState::Idle;

	TWeakObjectPtr<AActor> ThreatTarget;

	void SetState(EMMOCreatureAIState NewState);

	AMMOCreature* GetCreature() const;

	/** True if Target exists and is alive */
	static bool IsValidThreat(const AActor* Target);

	void TickIdle(AMMOCreature* Creature);
	void TickChasing(AMMOCreature* Creature);
	void TickAttacking(AMMOCreature* Creature, float DeltaSeconds);
	void TickReturning(AMMOCreature* Creature);

	/** True if the creature has strayed too far from home or lost its target */
	bool ShouldReturn(const AMMOCreature* Creature) const;

	void SteerToward(AMMOCreature* Creature, const FVector& Destination);
};

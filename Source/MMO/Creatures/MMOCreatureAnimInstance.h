// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "MMOCreatureAnimInstance.generated.h"

/**
 *  Base class for creature Animation Blueprints.
 *  Exposes the creature's gameplay state so the AnimBP state machine can drive Idle / Locomotion / Combat / Dead.
 *  Attack, hit-react and death are played as montages set on the creature (see AMMOCreature "Creature|Animation").
 */
UCLASS()
class UMMOCreatureAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:

	/** Ground speed in cm/s, for Idle <-> Walk/Run blending */
	UPROPERTY(BlueprintReadOnly, Category="Creature")
	float Speed = 0.0f;

	/** True while the creature is chasing or attacking */
	UPROPERTY(BlueprintReadOnly, Category="Creature")
	bool bInCombat = false;

	/** True while an attack is in progress */
	UPROPERTY(BlueprintReadOnly, Category="Creature")
	bool bIsAttacking = false;

	/** True once dead, until respawn */
	UPROPERTY(BlueprintReadOnly, Category="Creature")
	bool bIsDead = false;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};

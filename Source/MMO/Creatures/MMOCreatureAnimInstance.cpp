// Copyright Epic Games, Inc. All Rights Reserved.

#include "Creatures/MMOCreatureAnimInstance.h"
#include "Creatures/MMOCreature.h"

void UMMOCreatureAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (const AMMOCreature* Creature = Cast<AMMOCreature>(TryGetPawnOwner()))
	{
		Speed = Creature->GetVelocity().Size2D();
		bInCombat = Creature->IsInCombat();
		bIsAttacking = Creature->IsAttacking();
		bIsDead = Creature->IsDead();
	}
}

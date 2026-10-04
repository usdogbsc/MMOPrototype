// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/MMOCreature.h"
#include "MMOGreyWolf.generated.h"

/**
 *  Grey Wolf: the first prototype creature.
 *  Sets the wolf's stats and builds a placeholder quadruped body from engine basic shapes.
 *  Create a Blueprint child of this class to tune values or swap in a real mesh later.
 */
UCLASS()
class AMMOGreyWolf : public AMMOCreature
{
	GENERATED_BODY()

public:

	AMMOGreyWolf();
};

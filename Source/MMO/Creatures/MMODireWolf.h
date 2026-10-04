// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/MMOGreyWolf.h"
#include "MMODireWolf.generated.h"

/**
 *  Dire Wolf: a stronger Grey Wolf found deep in the woods.
 *  Same creature, AI and body as the Grey Wolf; only its configuration differs
 *  (level, health, damage, size, coat, XP and loot table).
 */
UCLASS()
class AMMODireWolf : public AMMOGreyWolf
{
	GENERATED_BODY()

public:

	AMMODireWolf();
};

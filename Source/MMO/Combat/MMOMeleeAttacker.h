// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MMOMeleeAttacker.generated.h"

/**
 *  MMOMeleeAttacker interface
 *  Implemented by anything whose melee damage is timed by its attack animation
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UMMOMeleeAttacker : public UInterface
{
	GENERATED_BODY()
};

class IMMOMeleeAttacker
{
	GENERATED_BODY()

public:

	/** Called on the frame the attack visually connects (usually by UMMOAnimNotify_MeleeHit) */
	virtual void NotifyMeleeHitFrame() = 0;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MMOTargetable.generated.h"

class UMMOHealthComponent;

/**
 *  MMOTargetable interface
 *  Implemented by anything the player can select as a combat target (creatures today, NPCs/players later)
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UMMOTargetable : public UInterface
{
	GENERATED_BODY()
};

class IMMOTargetable
{
	GENERATED_BODY()

public:

	/** Name shown in the target frame and nameplate */
	virtual FText GetTargetDisplayName() const = 0;

	/** Level shown in the target frame */
	virtual int32 GetTargetLevel() const { return 1; }

	/** Returns true if this can currently be selected as a new target */
	virtual bool IsTargetable() const = 0;

	/** Health component used by the target frame and combat rules */
	virtual UMMOHealthComponent* GetTargetHealth() const = 0;

	/** Called when the player selects or deselects this target */
	virtual void SetTargeted(bool bTargeted) = 0;

	/** World location for the overhead nameplate */
	virtual FVector GetNameplateLocation() const = 0;
};

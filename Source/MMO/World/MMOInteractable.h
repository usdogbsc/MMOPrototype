// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MMOInteractable.generated.h"

class AMMOCharacter;

/**
 *  MMOInteractable interface
 *  Anything the player can use with right-click / F that isn't a combat target:
 *  NPCs, vendors, gathering nodes, crafting stations...
 *  Implementers register themselves with MMOInteraction::Register in BeginPlay (and unregister in EndPlay)
 *  so cursor picking doesn't have to scan every actor in the world.
 */
UINTERFACE(MinimalAPI, NotBlueprintable)
class UMMOInteractable : public UInterface
{
	GENERATED_BODY()
};

class IMMOInteractable
{
	GENERATED_BODY()

public:

	/** Name shown in prompts */
	virtual FText GetInteractName() const = 0;

	/** True if the player may use it right now (ignoring distance) */
	virtual bool CanInteract(const AMMOCharacter* Player) const = 0;

	virtual void Interact(AMMOCharacter* Player) = 0;

	/** How close the player must be */
	virtual float GetInteractRange() const { return 400.0f; }

	/** Center and radius used for mouse picking */
	virtual FVector GetInteractLocation() const = 0;
	virtual float GetInteractPickRadius() const { return 70.0f; }
};

namespace MMOInteraction
{
	void Register(AActor* Actor);
	void Unregister(AActor* Actor);

	/** Registered interactables in World */
	TArray<AActor*> GetAll(const UWorld* World);
}

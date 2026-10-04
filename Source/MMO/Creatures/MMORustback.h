// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Creatures/MMOCreature.h"
#include "MMORustback.generated.h"

/**
 *  Rustback Skitterer: a rust-shelled beetle infesting the Rustvein Mine.
 *  Six-legged body built from basic shapes (mandibles on the jaw joint, abdomen on the tail joint).
 */
UCLASS()
class AMMORustback : public AMMOCreature
{
	GENERATED_BODY()

public:

	AMMORustback();

	/** Makes this a small summoned brood beetle (call before FinishSpawning) */
	void ConfigureAsBrood();
};

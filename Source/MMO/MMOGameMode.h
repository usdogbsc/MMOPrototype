// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "MMOGameMode.generated.h"

class AMMOCreature;

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AMMOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	/** Constructor */
	AMMOGameMode();

	virtual void StartPlay() override;

protected:

	/** If true and the level contains no creatures, spawns prototype creatures around the player start */
	UPROPERTY(EditAnywhere, Category="Prototype")
	bool bSpawnPrototypeCreatures = true;

	/** Creature class used for auto-spawning */
	UPROPERTY(EditAnywhere, Category="Prototype", meta=(EditCondition="bSpawnPrototypeCreatures"))
	TSubclassOf<AMMOCreature> PrototypeCreatureClass;

	/** Spawn offsets relative to the player start (X = forward, Y = right) */
	UPROPERTY(EditAnywhere, Category="Prototype", meta=(EditCondition="bSpawnPrototypeCreatures"))
	TArray<FVector> PrototypeCreatureOffsets;

	void SpawnPrototypeCreatures();
};

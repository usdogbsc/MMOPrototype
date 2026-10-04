// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Items/MMOItemTypes.h"
#include "MMOSaveGame.generated.h"

/** An item stored by id, so saves survive asset moves and don't hard-reference content */
USTRUCT()
struct FMMOSavedItem
{
	GENERATED_BODY()

	UPROPERTY()
	FName ItemId;

	UPROPERTY()
	int32 Quantity = 0;

	/** Backpack slot, or the equipment slot index for worn items */
	UPROPERTY()
	int32 Slot = INDEX_NONE;
};

USTRUCT()
struct FMMOSavedQuest
{
	GENERATED_BODY()

	UPROPERTY()
	FName QuestId;

	/** Kill/discover counts per objective (collect objectives are read from the backpack) */
	UPROPERTY()
	TArray<int32> Counts;
};

/**
 *  One character's progress on this PC: level, XP, health, position, backpack, worn gear,
 *  quests and discovered places. Written by UMMOSaveSubsystem.
 */
UCLASS()
class UMMOSaveGame : public USaveGame
{
	GENERATED_BODY()

public:

	static constexpr int32 CurrentVersion = 1;

	UPROPERTY()
	int32 Version = CurrentVersion;

	UPROPERTY()
	FDateTime SavedAt;

	/** Total seconds played across sessions */
	UPROPERTY()
	double PlayTime = 0.0;

	UPROPERTY()
	int32 Level = 1;

	UPROPERTY()
	int32 XP = 0;

	UPROPERTY()
	float Health = 0.0f;

	/** Map the position belongs to (position is ignored on other maps) */
	UPROPERTY()
	FString MapName;

	UPROPERTY()
	FVector Location = FVector::ZeroVector;

	UPROPERTY()
	float Yaw = 0.0f;

	UPROPERTY()
	bool bHasLocation = false;

	UPROPERTY()
	int32 Currency = 0;

	UPROPERTY()
	TArray<FMMOSavedItem> Inventory;

	UPROPERTY()
	TArray<FMMOSavedItem> Equipment;

	UPROPERTY()
	TArray<FMMOSavedQuest> ActiveQuests;

	UPROPERTY()
	TArray<FName> CompletedQuests;

	UPROPERTY()
	TArray<FName> Discovered;
};

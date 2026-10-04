// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/MMOItemTypes.h"
#include "MMOLootTable.generated.h"

/** One possible drop. Each entry is rolled independently */
USTRUCT(BlueprintType)
struct FMMOLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot")
	TObjectPtr<UMMOItemDefinition> Item;

	/** 0..1 chance per kill */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot", meta=(ClampMin=0, ClampMax=1))
	float DropChance = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot", meta=(ClampMin=1))
	int32 MinQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Loot", meta=(ClampMin=1))
	int32 MaxQuantity = 1;
};

/**
 *  Data asset listing what a creature can drop.
 *  Debug: "mmo.Loot.ChanceMultiplier <x>" scales all chances; "mmo.loot.force <ItemId>" adds an item to the next roll.
 */
UCLASS(BlueprintType)
class UMMOLootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TArray<FMMOLootEntry> Entries;

	/** Chance to drop a little currency */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot|Currency", meta=(ClampMin=0, ClampMax=1))
	float CurrencyChance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot|Currency", meta=(ClampMin=0))
	int32 MinCurrency = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot|Currency", meta=(ClampMin=0))
	int32 MaxCurrency = 0;

	/** Rolls the table into new item stacks (each with a fresh InstanceId) and a currency amount */
	void Roll(TArray<FMMOItemStack>& OutItems, int32& OutCurrency) const;

	/** Queues an item to be added to the next roll of any table (development aid) */
	static void ForceNextDrop(UMMOItemDefinition* Item, int32 Quantity = 1);

	static int32 GetPendingForcedDropCount();
};

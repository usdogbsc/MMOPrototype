// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOLootTable.h"
#include "Items/MMOItemDefinition.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<float> CVarLootChanceMultiplier(
	TEXT("mmo.Loot.ChanceMultiplier"),
	1.0f,
	TEXT("Multiplies every loot drop chance (development aid). 1 = normal, 100 = everything drops."));

namespace MMOLootDebug
{
	static TArray<FMMOItemStack> PendingForcedDrops;
}

void UMMOLootTable::Roll(TArray<FMMOItemStack>& OutItems, int32& OutCurrency) const
{
	OutItems.Reset();
	OutCurrency = 0;

	const float Multiplier = FMath::Max(0.0f, CVarLootChanceMultiplier.GetValueOnGameThread());

	for (const FMMOLootEntry& Entry : Entries)
	{
		if (!Entry.Item || FMath::FRand() >= Entry.DropChance * Multiplier)
		{
			continue;
		}

		const int32 Quantity = FMath::RandRange(Entry.MinQuantity, FMath::Max(Entry.MinQuantity, Entry.MaxQuantity));
		OutItems.Add(FMMOItemStack::Make(Entry.Item, FMath::Min(Quantity, Entry.Item->MaxStackSize)));
	}

	for (const FMMOItemStack& Forced : MMOLootDebug::PendingForcedDrops)
	{
		OutItems.Add(FMMOItemStack::Make(Forced.Item, Forced.Quantity));
	}
	MMOLootDebug::PendingForcedDrops.Reset();

	if (MaxCurrency > 0 && FMath::FRand() < CurrencyChance * Multiplier)
	{
		OutCurrency = FMath::RandRange(MinCurrency, FMath::Max(MinCurrency, MaxCurrency));
	}
}

void UMMOLootTable::ForceNextDrop(UMMOItemDefinition* Item, int32 Quantity)
{
	if (Item)
	{
		MMOLootDebug::PendingForcedDrops.Add(FMMOItemStack::Make(Item, FMath::Clamp(Quantity, 1, Item->MaxStackSize)));
	}
}

int32 UMMOLootTable::GetPendingForcedDropCount()
{
	return MMOLootDebug::PendingForcedDrops.Num();
}

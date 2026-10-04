// Copyright Epic Games, Inc. All Rights Reserved.

// Development-only console commands for testing loot, inventory and equipment.
//   mmo.items                       list item ids
//   mmo.give <ItemId> [Qty]         add an item to the backpack
//   mmo.loot.force <ItemId> [Qty]   add an item to the next corpse that rolls loot
//   mmo.Loot.ChanceMultiplier <x>   scale every drop chance (console variable)
//   mmo.inventory.fill [ItemId]     fill every empty slot with full stacks (default WolfFang)
//   mmo.inventory.clear             empty the backpack (equipment is kept)
//   mmo.currency <copper>           add currency

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectIterator.h"
#include "MMOCharacter.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOLootTable.h"
#include "MMO.h"

namespace MMOItemDebug
{
	static AMMOCharacter* GetPlayer(UWorld* World)
	{
		return World ? Cast<AMMOCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	}

	static UMMOItemDefinition* ParseItem(const TArray<FString>& Args, int32 Index, const TCHAR* Default = nullptr)
	{
		const FString Id = Args.IsValidIndex(Index) ? Args[Index] : FString(Default ? Default : TEXT(""));
		UMMOItemDefinition* Item = UMMOItemDefinition::FindById(FName(*Id));
		if (!Item)
		{
			UE_LOG(LogMMO, Warning, TEXT("Unknown item id '%s' (see mmo.items)"), *Id);
		}
		return Item;
	}

	static int32 ParseInt(const TArray<FString>& Args, int32 Index, int32 Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Default;
	}

	static void ListItems(const TArray<FString>& Args, UWorld* World)
	{
		// make sure the known prototype items are loaded
		for (const TCHAR* Id : { TEXT("WolfPelt"), TEXT("RawWolfMeat"), TEXT("WolfFang"), TEXT("WornLeatherBoots"), TEXT("Greyfang"), TEXT("TrainingSword") })
		{
			UMMOItemDefinition::FindById(Id);
		}
		for (TObjectIterator<UMMOItemDefinition> It; It; ++It)
		{
			if (!It->HasAnyFlags(RF_ClassDefaultObject))
			{
				UE_LOG(LogMMO, Display, TEXT("  %s  (%s, %s)"), *It->ItemId.ToString(), *It->DisplayName.ToString(), *MMOItems::GetRarityText(It->Rarity).ToString());
			}
		}
	}

	static void Give(const TArray<FString>& Args, UWorld* World)
	{
		AMMOCharacter* Player = GetPlayer(World);
		UMMOItemDefinition* Item = ParseItem(Args, 0);
		if (Player && Item)
		{
			const int32 Added = Player->GetInventory()->AddItem(Item, FMath::Max(1, ParseInt(Args, 1, 1)), true);
			UE_LOG(LogMMO, Display, TEXT("mmo.give: added %d x %s"), Added, *Item->ItemId.ToString());
		}
	}

	static void ForceDrop(const TArray<FString>& Args, UWorld* World)
	{
		if (UMMOItemDefinition* Item = ParseItem(Args, 0))
		{
			UMMOLootTable::ForceNextDrop(Item, FMath::Max(1, ParseInt(Args, 1, 1)));
			UE_LOG(LogMMO, Display, TEXT("mmo.loot.force: next corpse will contain %s"), *Item->ItemId.ToString());
		}
	}

	static void Fill(const TArray<FString>& Args, UWorld* World)
	{
		AMMOCharacter* Player = GetPlayer(World);
		UMMOItemDefinition* Item = ParseItem(Args, 0, TEXT("WolfFang"));
		if (!Player || !Item)
		{
			return;
		}

		UMMOInventoryComponent* Inventory = Player->GetInventory();
		int32 Filled = 0;
		for (int32 i = 0; i < Inventory->GetSlots().Num(); ++i)
		{
			if (Inventory->GetSlot(i).IsEmpty())
			{
				Inventory->AddStack(FMMOItemStack::Make(Item, Item->MaxStackSize), i);
				++Filled;
			}
		}
		UE_LOG(LogMMO, Display, TEXT("mmo.inventory.fill: filled %d slots with %s"), Filled, *Item->ItemId.ToString());
	}

	static void Clear(const TArray<FString>& Args, UWorld* World)
	{
		if (AMMOCharacter* Player = GetPlayer(World))
		{
			Player->GetInventory()->ClearInventory();
			UE_LOG(LogMMO, Display, TEXT("mmo.inventory.clear: backpack emptied"));
		}
	}

	static void Currency(const TArray<FString>& Args, UWorld* World)
	{
		if (AMMOCharacter* Player = GetPlayer(World))
		{
			Player->GetInventory()->AddCurrency(FMath::Max(1, ParseInt(Args, 0, 100)), true);
		}
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMMOItemsCommand(TEXT("mmo.items"), TEXT("Lists item ids."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOItemDebug::ListItems));

static FAutoConsoleCommandWithWorldAndArgs GMMOGiveCommand(TEXT("mmo.give"), TEXT("mmo.give <ItemId> [Qty]: adds an item to the backpack."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOItemDebug::Give));

static FAutoConsoleCommandWithWorldAndArgs GMMOForceDropCommand(TEXT("mmo.loot.force"), TEXT("mmo.loot.force <ItemId> [Qty]: adds an item to the next corpse's loot."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOItemDebug::ForceDrop));

static FAutoConsoleCommandWithWorldAndArgs GMMOFillCommand(TEXT("mmo.inventory.fill"), TEXT("mmo.inventory.fill [ItemId]: fills every empty backpack slot (default WolfFang)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOItemDebug::Fill));

static FAutoConsoleCommandWithWorldAndArgs GMMOClearCommand(TEXT("mmo.inventory.clear"), TEXT("Empties the backpack (equipment is kept)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOItemDebug::Clear));

static FAutoConsoleCommandWithWorldAndArgs GMMOCurrencyCommand(TEXT("mmo.currency"), TEXT("mmo.currency <copper>: adds currency."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOItemDebug::Currency));

#endif // !UE_BUILD_SHIPPING

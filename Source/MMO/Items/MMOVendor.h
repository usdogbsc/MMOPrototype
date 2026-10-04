// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/MMOItemTypes.h"
#include "MMOVendor.generated.h"

class UMMOInventoryComponent;
class UMMOItemDefinition;

/** One line of a vendor's stock */
USTRUCT(BlueprintType)
struct FMMOVendorEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vendor")
	TObjectPtr<UMMOItemDefinition> Item;

	/** Copper per item (0 = the item's buy price) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vendor", meta=(ClampMin=0))
	int32 Price = 0;

	int32 GetPrice() const;
};

UENUM()
enum class EMMOVendorResult : uint8
{
	Success,
	NotEnoughMoney,
	InventoryFull,
	CannotSell,
	InvalidItem,
	TooFar
};

/** Buy/sell rules shared by the character and tests (no world needed) */
namespace MMOVendor
{
	/** Buys Quantity of Item at Price each: checks money and space first, then pays and adds */
	EMMOVendorResult Buy(UMMOInventoryComponent* Inventory, UMMOItemDefinition* Item, int32 Price, int32 Quantity = 1);

	/** Sells a whole backpack stack for its sell value. OutSold receives the stack (for buyback) */
	EMMOVendorResult Sell(UMMOInventoryComponent* Inventory, int32 SlotIndex, FMMOItemStack& OutSold, int32& OutCopper);

	/** Copper paid when selling this stack */
	int32 GetSellPrice(const FMMOItemStack& Stack);

	FText GetResultText(EMMOVendorResult Result);
}

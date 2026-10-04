// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOVendor.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"

#define LOCTEXT_NAMESPACE "MMOVendor"

int32 FMMOVendorEntry::GetPrice() const
{
	return Price > 0 ? Price : (Item ? Item->GetBuyPrice() : 0);
}

namespace MMOVendor
{
	EMMOVendorResult Buy(UMMOInventoryComponent* Inventory, UMMOItemDefinition* Item, int32 Price, int32 Quantity)
	{
		if (!Inventory || !Item || Quantity <= 0)
		{
			return EMMOVendorResult::InvalidItem;
		}
		const int32 Cost = Price * Quantity;
		if (Inventory->GetCurrency() < Cost)
		{
			return EMMOVendorResult::NotEnoughMoney;
		}
		if (Inventory->GetAddableQuantity(Item, Quantity) < Quantity)
		{
			return EMMOVendorResult::InventoryFull;
		}
		Inventory->SpendCurrency(Cost);
		Inventory->AddItem(Item, Quantity, true);
		return EMMOVendorResult::Success;
	}

	int32 GetSellPrice(const FMMOItemStack& Stack)
	{
		return Stack.IsEmpty() ? 0 : Stack.Item->SellValue * Stack.Quantity;
	}

	EMMOVendorResult Sell(UMMOInventoryComponent* Inventory, int32 SlotIndex, FMMOItemStack& OutSold, int32& OutCopper)
	{
		OutCopper = 0;
		if (!Inventory || !Inventory->IsValidSlot(SlotIndex) || Inventory->GetSlot(SlotIndex).IsEmpty())
		{
			return EMMOVendorResult::InvalidItem;
		}
		const FMMOItemStack& Stack = Inventory->GetSlot(SlotIndex);
		if (Stack.Item->SellValue <= 0)
		{
			return EMMOVendorResult::CannotSell;
		}
		OutCopper = GetSellPrice(Stack);
		OutSold = Inventory->RemoveFromSlot(SlotIndex, Stack.Quantity);
		Inventory->AddCurrency(OutCopper, true);
		return EMMOVendorResult::Success;
	}

	FText GetResultText(EMMOVendorResult Result)
	{
		switch (Result)
		{
		case EMMOVendorResult::NotEnoughMoney: return LOCTEXT("NoMoney", "You don't have enough money.");
		case EMMOVendorResult::InventoryFull: return LOCTEXT("Full", "Inventory is full.");
		case EMMOVendorResult::CannotSell: return LOCTEXT("CannotSell", "The merchant has no use for that.");
		case EMMOVendorResult::TooFar: return LOCTEXT("TooFar", "You are too far away.");
		case EMMOVendorResult::InvalidItem: return LOCTEXT("Invalid", "Nothing to trade.");
		default: return FText::GetEmpty();
		}
	}
}

#undef LOCTEXT_NAMESPACE

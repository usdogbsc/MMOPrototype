// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOLootContainerComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOLootTable.h"

UMMOLootContainerComponent::UMMOLootContainerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMMOLootContainerComponent::GenerateFrom(const UMMOLootTable* Table)
{
	TArray<FMMOItemStack> Rolled;
	int32 RolledCurrency = 0;
	if (Table)
	{
		Table->Roll(Rolled, RolledCurrency);
	}
	SetLoot(Rolled, RolledCurrency);
}

void UMMOLootContainerComponent::SetLoot(const TArray<FMMOItemStack>& NewItems, int32 NewCurrency)
{
	Items.Reset();
	for (const FMMOItemStack& Stack : NewItems)
	{
		if (!Stack.IsEmpty())
		{
			Items.Add(Stack);
			if (!Items.Last().InstanceId.IsValid())
			{
				Items.Last().InstanceId = FGuid::NewGuid();
			}
		}
	}
	Currency = FMath::Max(0, NewCurrency);
	OnLootChanged.Broadcast();
}

void UMMOLootContainerComponent::ClearLoot()
{
	if (HasLoot())
	{
		Items.Reset();
		Currency = 0;
		OnLootChanged.Broadcast();
	}
}

EMMOLootResult UMMOLootContainerComponent::TakeItem(const FGuid& InstanceId, UMMOInventoryComponent* Inventory, FMMOItemStack* OutTaken)
{
	const int32 Index = Items.IndexOfByPredicate([&InstanceId](const FMMOItemStack& Stack) { return Stack.InstanceId == InstanceId; });
	if (Index == INDEX_NONE || !Inventory)
	{
		return EMMOLootResult::NotFound;
	}

	FMMOItemStack& Stack = Items[Index];

	// non-stackable items keep their identity when they move; stackables merge into existing stacks
	int32 Moved = 0;
	if (Stack.Item->IsStackable())
	{
		Moved = Inventory->AddItem(Stack.Item, Stack.Quantity, true);
	}
	else if (Inventory->AddStack(Stack))
	{
		Moved = Stack.Quantity;
		Inventory->OnItemsReceived.Broadcast(Stack.Item, Moved);
	}

	if (Moved <= 0)
	{
		return EMMOLootResult::InventoryFull;
	}

	if (OutTaken)
	{
		*OutTaken = Stack;
		OutTaken->Quantity = Moved;
	}

	Stack.Quantity -= Moved;
	const bool bAllTaken = Stack.Quantity <= 0;
	if (bAllTaken)
	{
		Items.RemoveAt(Index);
	}

	OnLootChanged.Broadcast();
	return bAllTaken ? EMMOLootResult::Success : EMMOLootResult::Partial;
}

EMMOLootResult UMMOLootContainerComponent::TakeCurrency(UMMOInventoryComponent* Inventory)
{
	if (!Inventory || Currency <= 0)
	{
		return EMMOLootResult::NotFound;
	}

	const int32 Amount = Currency;
	Currency = 0;
	Inventory->AddCurrency(Amount, true);
	OnLootChanged.Broadcast();
	return EMMOLootResult::Success;
}

EMMOLootResult UMMOLootContainerComponent::TakeAll(UMMOInventoryComponent* Inventory)
{
	if (!Inventory || !HasLoot())
	{
		return EMMOLootResult::NotFound;
	}

	bool bAnyTaken = false;
	bool bAnyLeft = false;

	if (Currency > 0)
	{
		bAnyTaken |= TakeCurrency(Inventory) == EMMOLootResult::Success;
	}

	// copy the ids first: TakeItem removes entries as it goes
	TArray<FGuid> Ids;
	for (const FMMOItemStack& Stack : Items)
	{
		Ids.Add(Stack.InstanceId);
	}

	for (const FGuid& Id : Ids)
	{
		const EMMOLootResult Result = TakeItem(Id, Inventory);
		bAnyTaken |= Result == EMMOLootResult::Success || Result == EMMOLootResult::Partial;
		bAnyLeft |= Result != EMMOLootResult::Success;
	}

	if (!bAnyLeft)
	{
		return EMMOLootResult::Success;
	}
	return bAnyTaken ? EMMOLootResult::Partial : EMMOLootResult::InventoryFull;
}

int32 UMMOLootContainerComponent::GetTotalItemCount() const
{
	int32 Count = 0;
	for (const FMMOItemStack& Stack : Items)
	{
		Count += Stack.Quantity;
	}
	return Count;
}

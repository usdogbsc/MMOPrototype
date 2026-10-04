// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"

namespace MMOInventory
{
	static const FMMOItemStack EmptyStack;
}

UMMOInventoryComponent::UMMOInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
}

void UMMOInventoryComponent::InitializeComponent()
{
	Super::InitializeComponent();

	EnsureSlots();
}

void UMMOInventoryComponent::EnsureSlots()
{
	if (Slots.Num() != Capacity)
	{
		Slots.SetNum(FMath::Max(1, Capacity));
	}
}

int32 UMMOInventoryComponent::GetAddableQuantity(const UMMOItemDefinition* Item, int32 Quantity) const
{
	if (!Item || Quantity <= 0)
	{
		return 0;
	}

	const int32 MaxStack = FMath::Max(1, Item->MaxStackSize);
	int32 Room = 0;

	for (const FMMOItemStack& Stack : Slots)
	{
		if (Stack.IsEmpty())
		{
			Room += MaxStack;
		}
		else if (Stack.Item == Item)
		{
			Room += FMath::Max(0, MaxStack - Stack.Quantity);
		}

		if (Room >= Quantity)
		{
			return Quantity;
		}
	}

	return Room;
}

int32 UMMOInventoryComponent::AddItem(UMMOItemDefinition* Item, int32 Quantity, bool bFromWorld)
{
	EnsureSlots();

	if (!Item || Quantity <= 0)
	{
		return 0;
	}

	const int32 MaxStack = FMath::Max(1, Item->MaxStackSize);
	int32 Remaining = Quantity;

	// top up existing stacks first
	if (MaxStack > 1)
	{
		for (FMMOItemStack& Stack : Slots)
		{
			if (Remaining <= 0)
			{
				break;
			}
			if (!Stack.IsEmpty() && Stack.Item == Item && Stack.Quantity < MaxStack)
			{
				const int32 Moved = FMath::Min(Remaining, MaxStack - Stack.Quantity);
				Stack.Quantity += Moved;
				Remaining -= Moved;
			}
		}
	}

	// then start new stacks in empty slots
	for (FMMOItemStack& Stack : Slots)
	{
		if (Remaining <= 0)
		{
			break;
		}
		if (Stack.IsEmpty())
		{
			const int32 Moved = FMath::Min(Remaining, MaxStack);
			Stack = FMMOItemStack::Make(Item, Moved);
			Remaining -= Moved;
		}
	}

	const int32 Added = Quantity - Remaining;
	if (Added > 0)
	{
		BroadcastChanged();
		if (bFromWorld)
		{
			OnItemsReceived.Broadcast(Item, Added);
		}
	}
	return Added;
}

bool UMMOInventoryComponent::AddStack(const FMMOItemStack& Stack, int32 PreferredSlot)
{
	EnsureSlots();

	if (Stack.IsEmpty())
	{
		return false;
	}

	int32 Target = (Slots.IsValidIndex(PreferredSlot) && Slots[PreferredSlot].IsEmpty()) ? PreferredSlot : FindFirstEmptySlot();
	if (Target == INDEX_NONE)
	{
		return false;
	}

	Slots[Target] = Stack;
	if (!Slots[Target].InstanceId.IsValid())
	{
		Slots[Target].InstanceId = FGuid::NewGuid();
	}
	BroadcastChanged();
	return true;
}

FMMOItemStack UMMOInventoryComponent::RemoveFromSlot(int32 SlotIndex, int32 Quantity)
{
	FMMOItemStack Removed;
	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsEmpty() || Quantity <= 0)
	{
		return Removed;
	}

	FMMOItemStack& Stack = Slots[SlotIndex];
	const int32 Taken = FMath::Min(Quantity, Stack.Quantity);

	Removed = Stack;
	Removed.Quantity = Taken;
	Stack.Quantity -= Taken;

	if (Stack.Quantity <= 0)
	{
		Stack.Reset();
	}
	else
	{
		// a split-off part is a new stack
		Removed.InstanceId = FGuid::NewGuid();
	}

	BroadcastChanged();
	return Removed;
}

FMMOItemStack UMMOInventoryComponent::ReplaceSlot(int32 SlotIndex, const FMMOItemStack& NewStack)
{
	FMMOItemStack Previous;
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return Previous;
	}

	Previous = Slots[SlotIndex];
	Slots[SlotIndex] = NewStack;
	if (Slots[SlotIndex].IsEmpty())
	{
		Slots[SlotIndex].Reset();
	}
	BroadcastChanged();
	return Previous;
}

bool UMMOInventoryComponent::MoveSlot(int32 FromIndex, int32 ToIndex)
{
	if (!Slots.IsValidIndex(FromIndex) || !Slots.IsValidIndex(ToIndex) || FromIndex == ToIndex || Slots[FromIndex].IsEmpty())
	{
		return false;
	}

	FMMOItemStack& From = Slots[FromIndex];
	FMMOItemStack& To = Slots[ToIndex];

	// merge into a matching stack
	if (!To.IsEmpty() && To.Item == From.Item && From.Item->IsStackable())
	{
		const int32 Moved = FMath::Min(From.Quantity, From.Item->MaxStackSize - To.Quantity);
		if (Moved > 0)
		{
			To.Quantity += Moved;
			From.Quantity -= Moved;
			if (From.Quantity <= 0)
			{
				From.Reset();
			}
			BroadcastChanged();
			return true;
		}
	}

	Swap(From, To);
	BroadcastChanged();
	return true;
}

int32 UMMOInventoryComponent::RemoveItem(const UMMOItemDefinition* Item, int32 Quantity)
{
	if (!Item || Quantity <= 0)
	{
		return 0;
	}

	int32 Remaining = Quantity;
	for (int32 i = Slots.Num() - 1; i >= 0 && Remaining > 0; --i)
	{
		FMMOItemStack& Stack = Slots[i];
		if (!Stack.IsEmpty() && Stack.Item == Item)
		{
			const int32 Taken = FMath::Min(Remaining, Stack.Quantity);
			Stack.Quantity -= Taken;
			Remaining -= Taken;
			if (Stack.Quantity <= 0)
			{
				Stack.Reset();
			}
		}
	}

	const int32 Removed = Quantity - Remaining;
	if (Removed > 0)
	{
		BroadcastChanged();
	}
	return Removed;
}

void UMMOInventoryComponent::AddCurrency(int32 Amount, bool bFromWorld)
{
	if (Amount <= 0)
	{
		return;
	}

	Currency += Amount;
	BroadcastChanged();
	if (bFromWorld)
	{
		OnCurrencyReceived.Broadcast(Amount);
	}
}

void UMMOInventoryComponent::ClearInventory()
{
	EnsureSlots();
	for (FMMOItemStack& Stack : Slots)
	{
		Stack.Reset();
	}
	BroadcastChanged();
}

int32 UMMOInventoryComponent::CountItem(const UMMOItemDefinition* Item) const
{
	int32 Count = 0;
	for (const FMMOItemStack& Stack : Slots)
	{
		if (!Stack.IsEmpty() && Stack.Item == Item)
		{
			Count += Stack.Quantity;
		}
	}
	return Count;
}

int32 UMMOInventoryComponent::GetFreeSlotCount() const
{
	int32 Count = 0;
	for (const FMMOItemStack& Stack : Slots)
	{
		Count += Stack.IsEmpty() ? 1 : 0;
	}
	return Count;
}

int32 UMMOInventoryComponent::FindFirstEmptySlot() const
{
	return Slots.IndexOfByPredicate([](const FMMOItemStack& Stack) { return Stack.IsEmpty(); });
}

const FMMOItemStack& UMMOInventoryComponent::GetSlot(int32 SlotIndex) const
{
	return Slots.IsValidIndex(SlotIndex) ? Slots[SlotIndex] : MMOInventory::EmptyStack;
}

int32 UMMOInventoryComponent::GetTotalItemCount() const
{
	int32 Count = 0;
	for (const FMMOItemStack& Stack : Slots)
	{
		Count += Stack.IsEmpty() ? 0 : Stack.Quantity;
	}
	return Count;
}

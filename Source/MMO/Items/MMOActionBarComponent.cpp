// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOActionBarComponent.h"

UMMOActionBarComponent::UMMOActionBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Slots.SetNum(NumSlots);
}

const FMMOActionSlot& UMMOActionBarComponent::GetSlot(int32 Index) const
{
	static const FMMOActionSlot Empty;
	return Slots.IsValidIndex(Index) ? Slots[Index] : Empty;
}

void UMMOActionBarComponent::SetSlot(int32 Index, const FMMOActionSlot& Action)
{
	if (!Slots.IsValidIndex(Index))
	{
		return;
	}
	const int32 Existing = FindSlot(Action);
	if (Existing != INDEX_NONE && Existing != Index)
	{
		Slots[Existing] = FMMOActionSlot();
	}
	Slots[Index] = Action;
	OnActionBarChanged.Broadcast();
}

void UMMOActionBarComponent::ClearSlot(int32 Index)
{
	if (Slots.IsValidIndex(Index) && !Slots[Index].IsEmpty())
	{
		Slots[Index] = FMMOActionSlot();
		OnActionBarChanged.Broadcast();
	}
}

void UMMOActionBarComponent::SwapSlots(int32 A, int32 B)
{
	if (Slots.IsValidIndex(A) && Slots.IsValidIndex(B) && A != B)
	{
		Slots.Swap(A, B);
		OnActionBarChanged.Broadcast();
	}
}

int32 UMMOActionBarComponent::FindSlot(const FMMOActionSlot& Action) const
{
	return Action.IsEmpty() ? INDEX_NONE : Slots.IndexOfByKey(Action);
}

int32 UMMOActionBarComponent::AutoPlace(const FMMOActionSlot& Action)
{
	if (Action.IsEmpty())
	{
		return INDEX_NONE;
	}
	const int32 Existing = FindSlot(Action);
	if (Existing != INDEX_NONE)
	{
		return Existing;
	}
	const int32 Free = Slots.IndexOfByPredicate([](const FMMOActionSlot& Slot) { return Slot.IsEmpty(); });
	if (Free != INDEX_NONE)
	{
		Slots[Free] = Action;
		OnActionBarChanged.Broadcast();
	}
	return Free;
}

void UMMOActionBarComponent::RestoreSlots(const TArray<FMMOActionSlot>& InSlots)
{
	Slots = InSlots;
	Slots.SetNum(NumSlots);
	OnActionBarChanged.Broadcast();
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"

namespace MMOEquipment
{
	static const FMMOItemStack EmptyStack;
}

UMMOEquipmentComponent::UMMOEquipmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;

	StartingEquipment.Add(TSoftObjectPtr<UMMOItemDefinition>(FSoftObjectPath(TEXT("/Game/MMO/Items/DA_Item_TrainingSword.DA_Item_TrainingSword"))));
}

void UMMOEquipmentComponent::InitializeComponent()
{
	Super::InitializeComponent();

	EnsureSlots();
}

void UMMOEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	for (const TSoftObjectPtr<UMMOItemDefinition>& ItemRef : StartingEquipment)
	{
		EquipDirect(ItemRef.LoadSynchronous());
	}
}

void UMMOEquipmentComponent::EnsureSlots()
{
	if (Equipped.Num() != static_cast<int32>(EMMOEquipmentSlot::Count))
	{
		Equipped.SetNum(static_cast<int32>(EMMOEquipmentSlot::Count));
	}
}

EMMOEquipResult UMMOEquipmentComponent::EquipFromInventory(UMMOInventoryComponent* Inventory, int32 InventorySlot)
{
	EnsureSlots();

	if (!Inventory || !Inventory->IsValidSlot(InventorySlot) || Inventory->GetSlot(InventorySlot).IsEmpty())
	{
		return EMMOEquipResult::EmptySlot;
	}

	const FMMOItemStack& Candidate = Inventory->GetSlot(InventorySlot);
	if (!Candidate.Item->IsEquippable() || Candidate.Quantity != 1)
	{
		return EMMOEquipResult::NotEquippable;
	}

	// swap: the worn item (or nothing) takes the inventory slot the new item leaves
	const int32 SlotIndex = static_cast<int32>(Candidate.Item->EquipmentSlot);
	const FMMOItemStack PreviouslyWorn = Equipped[SlotIndex];
	Equipped[SlotIndex] = Inventory->ReplaceSlot(InventorySlot, PreviouslyWorn);

	OnEquipmentChanged.Broadcast();
	return EMMOEquipResult::Success;
}

EMMOEquipResult UMMOEquipmentComponent::Unequip(EMMOEquipmentSlot Slot, UMMOInventoryComponent* Inventory, int32 PreferredSlot)
{
	EnsureSlots();

	if (!IsValidSlot(Slot) || !Inventory)
	{
		return EMMOEquipResult::EmptySlot;
	}

	FMMOItemStack& Worn = Equipped[static_cast<int32>(Slot)];
	if (Worn.IsEmpty())
	{
		return EMMOEquipResult::EmptySlot;
	}

	if (!Inventory->AddStack(Worn, PreferredSlot))
	{
		return EMMOEquipResult::InventoryFull;
	}

	Worn.Reset();
	OnEquipmentChanged.Broadcast();
	return EMMOEquipResult::Success;
}

void UMMOEquipmentComponent::ClearEquipment()
{
	EnsureSlots();
	for (FMMOItemStack& Worn : Equipped)
	{
		Worn.Reset();
	}
	OnEquipmentChanged.Broadcast();
}

bool UMMOEquipmentComponent::EquipDirect(UMMOItemDefinition* Item)
{
	EnsureSlots();

	if (!Item || !Item->IsEquippable())
	{
		return false;
	}

	FMMOItemStack& Worn = Equipped[static_cast<int32>(Item->EquipmentSlot)];
	if (!Worn.IsEmpty())
	{
		return false;
	}

	Worn = FMMOItemStack::Make(Item, 1);
	OnEquipmentChanged.Broadcast();
	return true;
}

const FMMOItemStack& UMMOEquipmentComponent::GetEquipped(EMMOEquipmentSlot Slot) const
{
	const int32 Index = static_cast<int32>(Slot);
	return (IsValidSlot(Slot) && Equipped.IsValidIndex(Index)) ? Equipped[Index] : MMOEquipment::EmptyStack;
}

FMMOStatModifiers UMMOEquipmentComponent::GetTotalStats() const
{
	FMMOStatModifiers Total;
	for (const FMMOItemStack& Worn : Equipped)
	{
		if (!Worn.IsEmpty())
		{
			Total += Worn.Item->Stats;
		}
	}
	return Total;
}

void UMMOEquipmentComponent::GetWeaponDamage(float& OutMin, float& OutMax) const
{
	const FMMOItemStack& Weapon = GetEquipped(EMMOEquipmentSlot::MainHand);
	if (!Weapon.IsEmpty() && Weapon.Item->IsWeapon())
	{
		OutMin = Weapon.Item->WeaponDamageMin;
		OutMax = FMath::Max(Weapon.Item->WeaponDamageMin, Weapon.Item->WeaponDamageMax);
		return;
	}

	OutMin = UnarmedDamageMin;
	OutMax = FMath::Max(UnarmedDamageMin, UnarmedDamageMax);
}

int32 UMMOEquipmentComponent::GetEquippedCount() const
{
	int32 Count = 0;
	for (const FMMOItemStack& Worn : Equipped)
	{
		Count += Worn.IsEmpty() ? 0 : 1;
	}
	return Count;
}

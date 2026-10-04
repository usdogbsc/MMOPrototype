// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/MMOItemTypes.h"
#include "MMOInventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMMOItemsReceivedSignature, UMMOItemDefinition*, Item, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMMOCurrencyReceivedSignature, int32, Amount);

/**
 *  Fixed-size backpack of item stacks plus a currency balance.
 *  All changes go through this component so a server-authoritative version can later own the same API.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOInventoryComponent();

	/** Number of backpack slots */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin=1, ClampMax=100))
	int32 Capacity = 20;

	/** Any slot or currency change */
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FMMOInventoryChangedSignature OnInventoryChanged;

	/** Items gained from the world (loot), for pickup feedback. Not fired for equip/unequip moves */
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FMMOItemsReceivedSignature OnItemsReceived;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FMMOCurrencyReceivedSignature OnCurrencyReceived;

protected:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	TArray<FMMOItemStack> Slots;

	/** Balance in copper */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory")
	int32 Currency = 0;

public:

	virtual void InitializeComponent() override;

	/** How much of Item could be added right now (existing stacks first, then empty slots) */
	int32 GetAddableQuantity(const UMMOItemDefinition* Item, int32 Quantity) const;

	/** Adds as much as fits. Returns the amount added. bFromWorld fires OnItemsReceived */
	int32 AddItem(UMMOItemDefinition* Item, int32 Quantity, bool bFromWorld = false);

	/** Places a whole stack (keeping its InstanceId) into an empty slot. Prefers PreferredSlot if empty */
	bool AddStack(const FMMOItemStack& Stack, int32 PreferredSlot = INDEX_NONE);

	/** Removes up to Quantity from a slot. Returns what was removed */
	FMMOItemStack RemoveFromSlot(int32 SlotIndex, int32 Quantity);

	/** Swaps the content of a slot with NewStack and returns the previous content */
	FMMOItemStack ReplaceSlot(int32 SlotIndex, const FMMOItemStack& NewStack);

	/** Moves a slot onto another: merges matching stackables, otherwise swaps */
	bool MoveSlot(int32 FromIndex, int32 ToIndex);

	/** Removes up to Quantity of Item across all stacks. Returns the amount removed */
	int32 RemoveItem(const UMMOItemDefinition* Item, int32 Quantity);

	void AddCurrency(int32 Amount, bool bFromWorld = false);

	void ClearInventory();

	/** Sets the purse directly (save games) */
	void SetCurrency(int32 Amount);

	int32 CountItem(const UMMOItemDefinition* Item) const;
	int32 GetFreeSlotCount() const;
	int32 FindFirstEmptySlot() const;
	bool IsValidSlot(int32 SlotIndex) const { return Slots.IsValidIndex(SlotIndex); }

	const FMMOItemStack& GetSlot(int32 SlotIndex) const;
	const TArray<FMMOItemStack>& GetSlots() const { return Slots; }
	int32 GetCurrency() const { return Currency; }

	/** Total quantity of every stack (for duplication checks in tests) */
	int32 GetTotalItemCount() const;

	/** Sizes the slot array to Capacity (called automatically; public for tests) */
	void EnsureSlots();

protected:

	void BroadcastChanged() { OnInventoryChanged.Broadcast(); }
};

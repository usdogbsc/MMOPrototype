// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/MMOItemTypes.h"
#include "MMOEquipmentComponent.generated.h"

class UMMOInventoryComponent;

UENUM(BlueprintType)
enum class EMMOEquipResult : uint8
{
	Success,
	NotEquippable,
	InventoryFull,
	EmptySlot
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOEquipmentChangedSignature);

/**
 *  Worn items, separate from the backpack. Items move between the two without ever existing in both:
 *  equipping swaps the inventory slot with the equipment slot; unequipping needs a free backpack slot.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOEquipmentComponent();

	/** Items equipped when play begins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
	TArray<TSoftObjectPtr<UMMOItemDefinition>> StartingEquipment;

	/** Damage range when no weapon is equipped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment", meta=(ClampMin=0))
	float UnarmedDamageMin = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment", meta=(ClampMin=0))
	float UnarmedDamageMax = 5.0f;

	UPROPERTY(BlueprintAssignable, Category="Equipment")
	FMMOEquipmentChangedSignature OnEquipmentChanged;

protected:

	/** Indexed by EMMOEquipmentSlot */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Equipment")
	TArray<FMMOItemStack> Equipped;

public:

	virtual void InitializeComponent() override;
	virtual void BeginPlay() override;

	/** Equips the item in an inventory slot, swapping any item already worn into that same inventory slot */
	EMMOEquipResult EquipFromInventory(UMMOInventoryComponent* Inventory, int32 InventorySlot);

	/** Moves a worn item back into the inventory (into PreferredSlot if it is empty) */
	EMMOEquipResult Unequip(EMMOEquipmentSlot Slot, UMMOInventoryComponent* Inventory, int32 PreferredSlot = INDEX_NONE);

	/** Equips an item that does not come from an inventory (starting gear). Fails if the slot is occupied */
	bool EquipDirect(UMMOItemDefinition* Item);

	/** Empties every slot without giving the items anywhere (save games) */
	void ClearEquipment();

	const FMMOItemStack& GetEquipped(EMMOEquipmentSlot Slot) const;

	/** Sum of all worn items' stat modifiers */
	FMMOStatModifiers GetTotalStats() const;

	/** Main-hand weapon damage, or the unarmed range */
	void GetWeaponDamage(float& OutMin, float& OutMax) const;

	int32 GetEquippedCount() const;

	/** Makes sure the slot array exists (called automatically; public for tests) */
	void EnsureSlots();

	static bool IsValidSlot(EMMOEquipmentSlot Slot) { return Slot != EMMOEquipmentSlot::None && Slot != EMMOEquipmentSlot::Count; }
};

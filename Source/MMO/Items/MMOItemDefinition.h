// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Items/MMOItemTypes.h"
#include "MMOItemDefinition.generated.h"

class UTexture2D;
class UStaticMesh;

/**
 *  Data asset describing one kind of item. Every item in the game is one of these assets (no per-item code).
 *  Assets live in /Game/MMO/Items and are named DA_Item_<ItemId>.
 */
UCLASS(BlueprintType)
class UMMOItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Stable identifier used by debug commands, loot tables and (later) saves */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(MultiLine=true))
	FText Description;

	/** Optional italic lore line */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(MultiLine=true))
	FText FlavorText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EMMOItemType ItemType = EMMOItemType::Miscellaneous;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	EMMOItemRarity Rarity = EMMOItemRarity::Common;

	/** Tooltip type line, e.g. "Crafting Material" or "One-Handed Sword" */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	FText CategoryText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item")
	TObjectPtr<UTexture2D> Icon;

	/** 1 = not stackable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(ClampMin=1))
	int32 MaxStackSize = 1;

	/** Placeholder value in copper, for future vendors */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item", meta=(ClampMin=0))
	int32 SellValue = 0;

	/** Slot this item equips into (None = not equippable) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
	EMMOEquipmentSlot EquipmentSlot = EMMOEquipmentSlot::None;

	/** Weapon damage range per swing (weapons only) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment", meta=(ClampMin=0))
	float WeaponDamageMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment", meta=(ClampMin=0))
	float WeaponDamageMax = 0.0f;

	/** Stat bonuses while equipped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment")
	FMMOStatModifiers Stats;

	/** Optional mesh shown on the character while equipped (attached to the slot's socket) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visual")
	TObjectPtr<UStaticMesh> EquippedMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visual")
	FTransform EquippedMeshTransform;

	/** Tint applied to EquippedMesh (if its material has a "Color" parameter) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visual")
	FLinearColor EquippedMeshColor = FLinearColor::White;

	bool IsStackable() const { return MaxStackSize > 1; }
	bool IsEquippable() const { return EquipmentSlot != EMMOEquipmentSlot::None && EquipmentSlot != EMMOEquipmentSlot::Count; }
	bool IsWeapon() const { return WeaponDamageMax > 0.0f; }

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("MMOItem"), GetFName()); }

	/** Finds an item definition by ItemId (loaded items first, then /Game/MMO/Items/DA_Item_<ItemId>) */
	static UMMOItemDefinition* FindById(FName InItemId);
};

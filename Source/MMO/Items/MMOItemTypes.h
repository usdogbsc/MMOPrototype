// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MMOItemTypes.generated.h"

class UMMOItemDefinition;

/** Broad item category */
UENUM(BlueprintType)
enum class EMMOItemType : uint8
{
	Material,
	Consumable,
	Weapon,
	Armor,
	Miscellaneous
};

/** Item quality tier. Drives name color everywhere items are shown */
UENUM(BlueprintType)
enum class EMMOItemRarity : uint8
{
	Common,
	Uncommon,
	Rare,
	Epic
};

/** Character equipment slots */
UENUM(BlueprintType)
enum class EMMOEquipmentSlot : uint8
{
	None,
	Head,
	Chest,
	Hands,
	Legs,
	Feet,
	MainHand,
	OffHand,
	Amulet,
	Ring,
	Count UMETA(Hidden)
};

/** Flat stat bonuses granted by an item while equipped */
USTRUCT(BlueprintType)
struct FMMOStatModifiers
{
	GENERATED_BODY()

	/** Reduces incoming physical damage (see UMMOHealthComponent::GetDamageReduction) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	float Armor = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	float MaxHealth = 0.0f;

	/** Flat damage added to every swing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stats")
	float AttackDamage = 0.0f;

	FMMOStatModifiers& operator+=(const FMMOStatModifiers& Other)
	{
		Armor += Other.Armor;
		MaxHealth += Other.MaxHealth;
		AttackDamage += Other.AttackDamage;
		return *this;
	}
};

/**
 *  A quantity of one item: the unit stored in inventories, equipment slots and loot containers.
 *  InstanceId identifies the stack so UI clicks and future server ownership can refer to it unambiguously.
 */
USTRUCT(BlueprintType)
struct FMMOItemStack
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item")
	TObjectPtr<UMMOItemDefinition> Item;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item")
	int32 Quantity = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Item")
	FGuid InstanceId;

	bool IsEmpty() const { return Item == nullptr || Quantity <= 0; }

	void Reset()
	{
		Item = nullptr;
		Quantity = 0;
		InstanceId.Invalidate();
	}

	static FMMOItemStack Make(UMMOItemDefinition* InItem, int32 InQuantity)
	{
		FMMOItemStack Stack;
		Stack.Item = InItem;
		Stack.Quantity = InQuantity;
		Stack.InstanceId = FGuid::NewGuid();
		return Stack;
	}
};

/** Presentation helpers shared by every item UI */
namespace MMOItems
{
	FLinearColor GetRarityColor(EMMOItemRarity Rarity);
	FText GetRarityText(EMMOItemRarity Rarity);
	FText GetSlotText(EMMOEquipmentSlot Slot);

	/** 1 silver = 100 copper, 1 gold = 100 silver. e.g. "1s 25c" */
	FString FormatCurrency(int32 Copper);
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/MMOItemTypes.h"
#include "MMOLootContainerComponent.generated.h"

class UMMOInventoryComponent;
class UMMOLootTable;

UENUM(BlueprintType)
enum class EMMOLootResult : uint8
{
	/** Everything requested was taken */
	Success,
	/** Some was taken, the rest did not fit */
	Partial,
	/** Nothing fit */
	InventoryFull,
	/** The item is no longer in the container */
	NotFound
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOLootChangedSignature);

/**
 *  Holds the loot of one corpse (or, later, a chest). Each container owns its own stacks, so corpses never share loot.
 *  Taking moves items into an inventory: only the amount that fits leaves the container, so nothing is lost or duplicated.
 */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOLootContainerComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UMMOLootContainerComponent();

	/** Display name for the loot window (e.g. "Grey Wolf") */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	FText ContainerName;

	UPROPERTY(BlueprintAssignable, Category="Loot")
	FMMOLootChangedSignature OnLootChanged;

protected:

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Loot")
	TArray<FMMOItemStack> Items;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Loot")
	int32 Currency = 0;

public:

	/** Replaces the contents with a fresh roll of Table */
	void GenerateFrom(const UMMOLootTable* Table);

	/** Replaces the contents directly */
	void SetLoot(const TArray<FMMOItemStack>& NewItems, int32 NewCurrency);

	/** Discards all contents (corpse despawned / respawned) */
	void ClearLoot();

	/** Takes as much of one stack as fits. OutTaken receives what moved */
	EMMOLootResult TakeItem(const FGuid& InstanceId, UMMOInventoryComponent* Inventory, FMMOItemStack* OutTaken = nullptr);

	EMMOLootResult TakeCurrency(UMMOInventoryComponent* Inventory);

	/** Takes everything that fits, leaving the rest */
	EMMOLootResult TakeAll(UMMOInventoryComponent* Inventory);

	bool HasLoot() const { return Items.Num() > 0 || Currency > 0; }
	const TArray<FMMOItemStack>& GetItems() const { return Items; }
	int32 GetCurrency() const { return Currency; }
	int32 GetTotalItemCount() const;
};

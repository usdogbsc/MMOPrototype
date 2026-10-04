// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MMOActionBarComponent.generated.h"

UENUM(BlueprintType)
enum class EMMOActionType : uint8
{
	None,
	/** Uses the item with this ItemId from the backpack */
	Item,
	/** Casts the ability with this AbilityId */
	Ability
};

USTRUCT(BlueprintType)
struct FMMOActionSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action Bar")
	EMMOActionType Type = EMMOActionType::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Action Bar")
	FName Id;

	bool IsEmpty() const { return Type == EMMOActionType::None || Id.IsNone(); }
	bool operator==(const FMMOActionSlot& Other) const { return Type == Other.Type && Id == Other.Id; }
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMMOActionBarChangedSignature);

/** The player's hotbar after Basic Attack: keys 2-9 hold items (and abilities) */
UCLASS(ClassGroup=(MMO), meta=(BlueprintSpawnableComponent))
class UMMOActionBarComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	static constexpr int32 NumSlots = 8;

	UMMOActionBarComponent();

	UPROPERTY(BlueprintAssignable, Category="Action Bar")
	FMMOActionBarChangedSignature OnActionBarChanged;

	const FMMOActionSlot& GetSlot(int32 Index) const;
	const TArray<FMMOActionSlot>& GetSlots() const { return Slots; }

	/** Puts an action in a slot (removing it from any other slot first) */
	void SetSlot(int32 Index, const FMMOActionSlot& Action);
	void ClearSlot(int32 Index);

	/** Swaps two slots (drag between slots) */
	void SwapSlots(int32 A, int32 B);

	int32 FindSlot(const FMMOActionSlot& Action) const;

	/** Places the action in the first empty slot unless it is already on the bar. Returns its slot, or INDEX_NONE if the bar is full */
	int32 AutoPlace(const FMMOActionSlot& Action);

	/** Replaces every slot (save games) */
	void RestoreSlots(const TArray<FMMOActionSlot>& InSlots);

	static FMMOActionSlot MakeItem(FName ItemId) { FMMOActionSlot Slot; Slot.Type = EMMOActionType::Item; Slot.Id = ItemId; return Slot; }
	static FMMOActionSlot MakeAbility(FName AbilityId) { FMMOActionSlot Slot; Slot.Type = EMMOActionType::Ability; Slot.Id = AbilityId; return Slot; }

protected:

	UPROPERTY(VisibleInstanceOnly, Category="Action Bar")
	TArray<FMMOActionSlot> Slots;
};

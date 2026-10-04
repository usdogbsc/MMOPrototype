// Copyright Epic Games, Inc. All Rights Reserved.

#include "Items/MMOItemDefinition.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "MMOItems"

UMMOItemDefinition* UMMOItemDefinition::FindById(FName InItemId)
{
	if (InItemId.IsNone())
	{
		return nullptr;
	}

	// ids are looked up often (quest progress, markers), so remember what we found
	static TMap<FName, TWeakObjectPtr<UMMOItemDefinition>> Cache;
	if (const TWeakObjectPtr<UMMOItemDefinition>* Cached = Cache.Find(InItemId))
	{
		if (Cached->IsValid() && (*Cached)->ItemId == InItemId)
		{
			return Cached->Get();
		}
		Cache.Remove(InItemId);
	}

	for (TObjectIterator<UMMOItemDefinition> It; It; ++It)
	{
		if (It->ItemId == InItemId && !It->HasAnyFlags(RF_ClassDefaultObject))
		{
			Cache.Add(InItemId, *It);
			return *It;
		}
	}

	const FString Name = FString::Printf(TEXT("DA_Item_%s"), *InItemId.ToString());
	UMMOItemDefinition* Loaded = LoadObject<UMMOItemDefinition>(nullptr, *FString::Printf(TEXT("/Game/MMO/Items/%s.%s"), *Name, *Name), nullptr, LOAD_NoWarn);
	if (Loaded)
	{
		Cache.Add(InItemId, Loaded);
	}
	return Loaded;
}

namespace MMOItems
{
	FLinearColor GetRarityColor(EMMOItemRarity Rarity)
	{
		switch (Rarity)
		{
		case EMMOItemRarity::Uncommon:	return FLinearColor(0.25f, 0.85f, 0.3f);
		case EMMOItemRarity::Rare:		return FLinearColor(0.25f, 0.55f, 1.0f);
		case EMMOItemRarity::Epic:		return FLinearColor(0.72f, 0.38f, 1.0f);
		default:						return FLinearColor(0.92f, 0.92f, 0.9f);
		}
	}

	FText GetRarityText(EMMOItemRarity Rarity)
	{
		switch (Rarity)
		{
		case EMMOItemRarity::Uncommon:	return LOCTEXT("Uncommon", "Uncommon");
		case EMMOItemRarity::Rare:		return LOCTEXT("Rare", "Rare");
		case EMMOItemRarity::Epic:		return LOCTEXT("Epic", "Epic");
		default:						return LOCTEXT("Common", "Common");
		}
	}

	FText GetSlotText(EMMOEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EMMOEquipmentSlot::Head:		return LOCTEXT("Head", "Head");
		case EMMOEquipmentSlot::Chest:		return LOCTEXT("Chest", "Chest");
		case EMMOEquipmentSlot::Hands:		return LOCTEXT("Hands", "Hands");
		case EMMOEquipmentSlot::Legs:		return LOCTEXT("Legs", "Legs");
		case EMMOEquipmentSlot::Feet:		return LOCTEXT("Feet", "Feet");
		case EMMOEquipmentSlot::MainHand:	return LOCTEXT("MainHand", "Main Hand");
		case EMMOEquipmentSlot::OffHand:	return LOCTEXT("OffHand", "Off Hand");
		case EMMOEquipmentSlot::Amulet:		return LOCTEXT("Amulet", "Amulet");
		case EMMOEquipmentSlot::Ring:		return LOCTEXT("Ring", "Ring");
		default:							return FText::GetEmpty();
		}
	}

	FString FormatCurrency(int32 Copper)
	{
		Copper = FMath::Max(0, Copper);
		const int32 Gold = Copper / 10000;
		const int32 Silver = (Copper / 100) % 100;
		const int32 Rest = Copper % 100;

		TArray<FString> Parts;
		if (Gold > 0) { Parts.Add(FString::Printf(TEXT("%dg"), Gold)); }
		if (Silver > 0) { Parts.Add(FString::Printf(TEXT("%ds"), Silver)); }
		if (Rest > 0 || Parts.IsEmpty()) { Parts.Add(FString::Printf(TEXT("%dc"), Rest)); }
		return FString::Join(Parts, TEXT(" "));
	}
}

#undef LOCTEXT_NAMESPACE

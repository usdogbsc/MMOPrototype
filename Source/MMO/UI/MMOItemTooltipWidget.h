// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/MMOItemTypes.h"
#include "MMOItemTooltipWidget.generated.h"

class UVerticalBox;

/**
 *  Item tooltip: rarity-colored name, type line, stats, description, flavor text, sell value,
 *  and a short "Currently equipped" comparison for equippable items.
 */
UCLASS()
class UMMOItemTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Fills the tooltip. Compare is the item currently worn in the same slot (may be null or empty) */
	void SetItem(const FMMOItemStack& Stack, const FMMOItemStack* Compare);

	/** Stat lines for an item ("Damage: 10 - 14", "+8 Armor", ...) */
	static TArray<FString> GetStatLines(const UMMOItemDefinition* Item);

protected:

	UPROPERTY()
	TObjectPtr<UVerticalBox> Content;

	virtual void NativeOnInitialized() override;

	void AddLine(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false, float TopPadding = 0.0f);
};

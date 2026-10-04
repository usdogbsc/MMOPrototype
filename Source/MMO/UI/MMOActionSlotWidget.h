// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMOActionSlotWidget.generated.h"

class AMMOCharacter;
class UImage;
class USizeBox;
class UTextBlock;
class UMMOItemSlotWidget;
class UMMOAbilityTooltipWidget;

/**
 *  One hotbar button (keys 2-9): an item (with backpack count) or an ability, with a cooldown sweep.
 *  Click to use, right-click to clear, drop a usable item or an ability on it to assign it.
 */
UCLASS()
class UMMOActionSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Setup(AMMOCharacter* InCharacter, int32 InIndex, const FString& KeyLabel);

	int32 GetIndex() const { return Index; }

	/** Count shown on the button (for tests) */
	int32 GetShownCount() const { return ShownCount; }

	static constexpr float Size = 50.0f;

protected:

	UPROPERTY()
	TObjectPtr<UMMOItemSlotWidget> ItemSlot;

	UPROPERTY()
	TObjectPtr<UImage> AbilityIcon;

	UPROPERTY()
	TObjectPtr<UTextBlock> AbilityInitials;

	UPROPERTY()
	TObjectPtr<UMMOAbilityTooltipWidget> AbilityTooltip;

	UPROPERTY()
	TObjectPtr<USizeBox> CooldownShade;

	UPROPERTY()
	TObjectPtr<UTextBlock> CooldownText;

	UPROPERTY()
	TObjectPtr<UTextBlock> KeyText;

	TWeakObjectPtr<AMMOCharacter> Character;
	int32 Index = INDEX_NONE;
	int32 ShownCount = -1;
	FName ShownId;
	uint8 ShownType = 0;

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	void Refresh();
	void SetCooldown(float Remaining, float Duration);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "Items/MMOItemTypes.h"
#include "MMOItemSlotWidget.generated.h"

class UBorder;
class UImage;
class UTextBlock;
class UMMOItemSlotWidget;
class UMMOItemTooltipWidget;

/** Where a slot widget lives, so drops know what to do */
UENUM()
enum class EMMOItemSlotContext : uint8
{
	Inventory,
	Equipment,
	Loot,
	Display
};

DECLARE_DELEGATE_OneParam(FMMOItemSlotClicked, UMMOItemSlotWidget* /*Slot*/);
DECLARE_DELEGATE_TwoParams(FMMOItemSlotDropped, UMMOItemSlotWidget* /*Target*/, UMMOItemSlotWidget* /*Source*/);

/** Drag payload: the slot the item was picked up from */
UCLASS()
class UMMOItemDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:

	TWeakObjectPtr<UMMOItemSlotWidget> SourceSlot;
};

/**
 *  One item square: icon (or abbreviation if no icon), quantity, rarity border, tooltip.
 *  Right-click / double-click and drag-and-drop are reported to the owning window through delegates.
 */
UCLASS()
class UMMOItemSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Configure(EMMOItemSlotContext InContext, int32 InSlotIndex, EMMOEquipmentSlot InEquipSlot = EMMOEquipmentSlot::None, const FText& InEmptyLabel = FText::GetEmpty(), float InSize = 50.0f);

	/** Shows a stack. Compare = item worn in the same slot, for the tooltip comparison */
	void SetStack(const FMMOItemStack& InStack, const FMMOItemStack* Compare = nullptr);

	const FMMOItemStack& GetStack() const { return Stack; }
	EMMOItemSlotContext GetContext() const { return Context; }
	int32 GetSlotIndex() const { return SlotIndex; }
	EMMOEquipmentSlot GetEquipSlot() const { return EquipSlot; }

	FMMOItemSlotClicked OnRightClicked;
	FMMOItemSlotClicked OnDoubleClicked;
	FMMOItemSlotDropped OnDropped;

protected:

	UPROPERTY()
	TObjectPtr<UBorder> Background;

	UPROPERTY()
	TObjectPtr<UImage> Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> FallbackText;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuantityText;

	UPROPERTY()
	TObjectPtr<UTextBlock> EmptyText;

	UPROPERTY()
	TObjectPtr<UMMOItemTooltipWidget> Tooltip;

	FMMOItemStack Stack;
	EMMOItemSlotContext Context = EMMOItemSlotContext::Display;
	int32 SlotIndex = INDEX_NONE;
	EMMOEquipmentSlot EquipSlot = EMMOEquipmentSlot::None;
	float SlotSize = 50.0f;
	bool bHovered = false;

	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	void BuildDefaultLayout();
	void RefreshBorder();
};

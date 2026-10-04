// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOItemSlotWidget.h"
#include "UI/MMOItemTooltipWidget.h"
#include "UI/MMOUIStyle.h"
#include "Items/MMOItemDefinition.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

namespace MMOItemSlot
{
	static const FLinearColor SlotColor(0.06f, 0.06f, 0.07f, 0.95f);
	static const FLinearColor EmptyOutline(0.0f, 0.0f, 0.0f, 0.8f);

	/** "Wolf Pelt" -> "WP" */
	static FString Abbreviate(const FText& Name)
	{
		FString Result;
		TArray<FString> Words;
		Name.ToString().ParseIntoArrayWS(Words);
		for (const FString& Word : Words)
		{
			if (!Word.IsEmpty() && Result.Len() < 2)
			{
				Result.AppendChar(FChar::ToUpper(Word[0]));
			}
		}
		return Result;
	}
}

void UMMOItemSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
}

void UMMOItemSlotWidget::BuildDefaultLayout()
{
	using namespace MMOUI;

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SlotSize"));
	Root->SetWidthOverride(SlotSize);
	Root->SetHeightOverride(SlotSize);
	WidgetTree->RootWidget = Root;

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	Root->AddChild(Layers);

	auto Fill = [Layers](UWidget* Child, const FMargin& LayerPadding = FMargin(0.0f))
	{
		UOverlaySlot* LayerSlot = Layers->AddChildToOverlay(Child);
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
		LayerSlot->SetPadding(LayerPadding);
	};

	Background = MakePanel(WidgetTree, MMOItemSlot::SlotColor, 5.0f, FMargin(0.0f), MMOItemSlot::EmptyOutline, 2.0f);
	Fill(Background);

	Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	Icon->SetVisibility(ESlateVisibility::Hidden);
	Fill(Icon, FMargin(4.0f));

	FallbackText = MakeText(WidgetTree, TEXT(""), 15, FLinearColor::White, true, ETextJustify::Center);
	UOverlaySlot* FallbackSlot = Layers->AddChildToOverlay(FallbackText);
	FallbackSlot->SetHorizontalAlignment(HAlign_Center);
	FallbackSlot->SetVerticalAlignment(VAlign_Center);

	EmptyText = MakeText(WidgetTree, TEXT(""), 9, FLinearColor(1.0f, 1.0f, 1.0f, 0.3f), false, ETextJustify::Center);
	UOverlaySlot* EmptySlot = Layers->AddChildToOverlay(EmptyText);
	EmptySlot->SetHorizontalAlignment(HAlign_Center);
	EmptySlot->SetVerticalAlignment(VAlign_Center);

	QuantityText = MakeText(WidgetTree, TEXT(""), 12, FLinearColor::White, true, ETextJustify::Right);
	UOverlaySlot* QuantitySlot = Layers->AddChildToOverlay(QuantityText);
	QuantitySlot->SetHorizontalAlignment(HAlign_Right);
	QuantitySlot->SetVerticalAlignment(VAlign_Bottom);
	QuantitySlot->SetPadding(FMargin(0.0f, 0.0f, 4.0f, 1.0f));
}

void UMMOItemSlotWidget::Configure(EMMOItemSlotContext InContext, int32 InSlotIndex, EMMOEquipmentSlot InEquipSlot, const FText& InEmptyLabel, float InSize)
{
	Context = InContext;
	SlotIndex = InSlotIndex;
	EquipSlot = InEquipSlot;
	SlotSize = InSize;

	if (USizeBox* Root = Cast<USizeBox>(WidgetTree ? WidgetTree->RootWidget : nullptr))
	{
		Root->SetWidthOverride(SlotSize);
		Root->SetHeightOverride(SlotSize);
	}
	if (EmptyText)
	{
		EmptyText->SetText(InEmptyLabel);
	}
}

void UMMOItemSlotWidget::SetStack(const FMMOItemStack& InStack, const FMMOItemStack* Compare)
{
	Stack = InStack;
	const bool bHasItem = !Stack.IsEmpty();
	UTexture2D* Texture = bHasItem ? Stack.Item->Icon.Get() : nullptr;

	if (Icon)
	{
		if (Texture)
		{
			Icon->SetBrushFromTexture(Texture);
		}
		Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (FallbackText)
	{
		FallbackText->SetText(FText::FromString(bHasItem && !Texture ? MMOItemSlot::Abbreviate(Stack.Item->DisplayName) : FString()));
		FallbackText->SetColorAndOpacity(FSlateColor(bHasItem ? MMOItems::GetRarityColor(Stack.Item->Rarity) : FLinearColor::White));
	}
	if (QuantityText)
	{
		QuantityText->SetText(FText::FromString(bHasItem && Stack.Quantity > 1 ? FString::FromInt(Stack.Quantity) : FString()));
	}
	if (EmptyText)
	{
		EmptyText->SetVisibility(bHasItem ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
	}

	// tooltip
	if (bHasItem)
	{
		if (!Tooltip)
		{
			Tooltip = CreateWidget<UMMOItemTooltipWidget>(this, UMMOItemTooltipWidget::StaticClass());
		}
		Tooltip->SetItem(Stack, Compare);
		SetToolTip(Tooltip);
	}
	else
	{
		SetToolTip(nullptr);
	}

	RefreshBorder();
}

void UMMOItemSlotWidget::RefreshBorder()
{
	if (!Background)
	{
		return;
	}

	FLinearColor Outline = Stack.IsEmpty() ? MMOItemSlot::EmptyOutline : MMOItems::GetRarityColor(Stack.Item->Rarity) * 0.85f;
	Outline.A = 1.0f;
	if (bHovered && Context != EMMOItemSlotContext::Display)
	{
		Outline = MMOUI::Colors::Gold;
	}
	Background->SetBrush(MMOUI::RoundedBrush(bHovered ? FLinearColor(0.12f, 0.11f, 0.1f, 0.97f) : MMOItemSlot::SlotColor, 5.0f, Outline, 2.0f));
}

FReply UMMOItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Context == EMMOItemSlotContext::Display)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnRightClicked.ExecuteIfBound(this);
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !Stack.IsEmpty() && Context != EMMOItemSlotContext::Loot)
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}

	return FReply::Handled();
}

FReply UMMOItemSlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (Context != EMMOItemSlotContext::Display && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		OnDoubleClicked.ExecuteIfBound(this);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
}

void UMMOItemSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (Stack.IsEmpty())
	{
		return;
	}

	UMMOItemDragOperation* Operation = NewObject<UMMOItemDragOperation>(this);
	Operation->SourceSlot = this;
	Operation->Pivot = EDragPivot::CenterCenter;

	UMMOItemSlotWidget* Visual = CreateWidget<UMMOItemSlotWidget>(this, UMMOItemSlotWidget::StaticClass());
	Visual->Configure(EMMOItemSlotContext::Display, INDEX_NONE, EMMOEquipmentSlot::None, FText::GetEmpty(), SlotSize);
	Visual->SetStack(Stack);
	Visual->SetRenderOpacity(0.85f);
	Operation->DefaultDragVisual = Visual;

	OutOperation = Operation;
}

bool UMMOItemSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UMMOItemDragOperation* Operation = Cast<UMMOItemDragOperation>(InOperation);
	UMMOItemSlotWidget* Source = Operation ? Operation->SourceSlot.Get() : nullptr;
	if (!Source || Source == this || Context == EMMOItemSlotContext::Display || Context == EMMOItemSlotContext::Loot)
	{
		return false;
	}

	OnDropped.ExecuteIfBound(this, Source);
	return true;
}

void UMMOItemSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	bHovered = true;
	RefreshBorder();
}

void UMMOItemSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bHovered = false;
	RefreshBorder();
}

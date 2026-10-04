// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOActionSlotWidget.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "Combat/MMOCooldownComponent.h"
#include "Items/MMOActionBarComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"

void UMMOActionSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		WidgetTree->RootWidget = Layers;

		ItemSlot = WidgetTree->ConstructWidget<UMMOItemSlotWidget>(UMMOItemSlotWidget::StaticClass());
		ItemSlot->Configure(EMMOItemSlotContext::Display, INDEX_NONE, EMMOEquipmentSlot::None, FText::GetEmpty(), Size);
		Layers->AddChildToOverlay(ItemSlot);

		// cooldown: a dark shade that shrinks from the top as the cooldown runs out
		CooldownShade = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CooldownShade->SetWidthOverride(Size - 4.0f);
		CooldownShade->SetHeightOverride(0.0f);
		UBorder* Shade = MMOUI::MakePanel(WidgetTree, FLinearColor(0.0f, 0.0f, 0.0f, 0.62f), 3.0f, FMargin(0.0f));
		CooldownShade->AddChild(Shade);
		UOverlaySlot* ShadeSlot = Layers->AddChildToOverlay(CooldownShade);
		ShadeSlot->SetHorizontalAlignment(HAlign_Center);
		ShadeSlot->SetVerticalAlignment(VAlign_Bottom);
		ShadeSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 2.0f));
		CooldownShade->SetVisibility(ESlateVisibility::HitTestInvisible);

		CooldownText = MMOUI::MakeText(WidgetTree, TEXT(""), 15, FLinearColor(1.0f, 0.95f, 0.7f), true, ETextJustify::Center);
		UOverlaySlot* TimeSlot = Layers->AddChildToOverlay(CooldownText);
		TimeSlot->SetHorizontalAlignment(HAlign_Center);
		TimeSlot->SetVerticalAlignment(VAlign_Center);
		CooldownText->SetVisibility(ESlateVisibility::HitTestInvisible);

		KeyText = MMOUI::MakeText(WidgetTree, TEXT(""), 11, MMOUI::Colors::Gold, true);
		UOverlaySlot* KeySlot = Layers->AddChildToOverlay(KeyText);
		KeySlot->SetHorizontalAlignment(HAlign_Left);
		KeySlot->SetVerticalAlignment(VAlign_Top);
		KeySlot->SetPadding(FMargin(5.0f, 2.0f, 0.0f, 0.0f));
		KeyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UMMOActionSlotWidget::Setup(AMMOCharacter* InCharacter, int32 InIndex, const FString& KeyLabel)
{
	Character = InCharacter;
	Index = InIndex;
	if (KeyText)
	{
		KeyText->SetText(FText::FromString(KeyLabel));
	}
	ShownCount = -1;
	ShownId = NAME_None;
	Refresh();
}

void UMMOActionSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	Refresh();
}

void UMMOActionSlotWidget::Refresh()
{
	AMMOCharacter* Owner = Character.Get();
	if (!Owner || !ItemSlot)
	{
		return;
	}

	const FMMOActionSlot& Action = Owner->GetActionBar()->GetSlot(Index);
	UMMOItemDefinition* Item = Action.Type == EMMOActionType::Item ? UMMOItemDefinition::FindById(Action.Id) : nullptr;
	const int32 Count = Item ? Owner->GetInventory()->CountItem(Item) : 0;

	// only rebuild the icon/tooltip when something changed
	if (Action.Id != ShownId || Count != ShownCount)
	{
		ShownId = Action.Id;
		ShownCount = Count;
		ItemSlot->SetStack(Item ? FMMOItemStack::Make(Item, FMath::Max(Count, 1)) : FMMOItemStack());
		ItemSlot->SetRenderOpacity(Item && Count == 0 ? 0.35f : 1.0f);
	}

	const FName CooldownKey = Item ? Item->GetCooldownKey() : NAME_None;
	const float Remaining = Item ? Owner->GetCooldowns()->GetRemaining(CooldownKey) : 0.0f;
	const float Duration = Item ? Owner->GetCooldowns()->GetDuration(CooldownKey) : 0.0f;
	const float Fraction = Duration > 0.0f ? FMath::Clamp(Remaining / Duration, 0.0f, 1.0f) : 0.0f;
	CooldownShade->SetHeightOverride((Size - 4.0f) * Fraction);
	CooldownText->SetText(Remaining > 0.0f ? FText::FromString(Remaining >= 60.0f ? FString::Printf(TEXT("%dm"), FMath::CeilToInt(Remaining / 60.0f)) : FString::FromInt(FMath::CeilToInt(Remaining))) : FText::GetEmpty());
}

FReply UMMOActionSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	AMMOCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return FReply::Handled();
	}

	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		Owner->UseActionSlot(Index);
	}
	else if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		Owner->GetActionBar()->ClearSlot(Index);
	}
	return FReply::Handled();
}

bool UMMOActionSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	const UMMOItemDragOperation* Operation = Cast<UMMOItemDragOperation>(InOperation);
	const UMMOItemSlotWidget* Source = Operation ? Operation->SourceSlot.Get() : nullptr;
	AMMOCharacter* Owner = Character.Get();
	if (!Owner || !Source || Source->GetStack().IsEmpty())
	{
		return false;
	}

	const UMMOItemDefinition* Item = Source->GetStack().Item;
	if (!Item->IsUsable())
	{
		Owner->ShowPlayerMessage(NSLOCTEXT("MMOItems", "NotUsableOnBar", "Only usable items go on the hotbar."));
		return true;
	}
	Owner->GetActionBar()->SetSlot(Index, UMMOActionBarComponent::MakeItem(Item->ItemId));
	return true;
}

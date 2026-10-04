// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOActionSlotWidget.h"
#include "UI/MMOAbilityWidgets.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "Combat/MMOAbilityComponent.h"
#include "Combat/MMOAbilityDefinition.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOCooldownComponent.h"
#include "Items/MMOActionBarComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"

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

		AbilityIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		UOverlaySlot* IconSlot = Layers->AddChildToOverlay(AbilityIcon);
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
		IconSlot->SetPadding(FMargin(4.0f));
		AbilityIcon->SetVisibility(ESlateVisibility::Hidden);

		AbilityInitials = MMOUI::MakeText(WidgetTree, TEXT(""), 15, FLinearColor(1.0f, 0.82f, 0.4f), true, ETextJustify::Center);
		UOverlaySlot* InitialsSlot = Layers->AddChildToOverlay(AbilityInitials);
		InitialsSlot->SetHorizontalAlignment(HAlign_Center);
		InitialsSlot->SetVerticalAlignment(VAlign_Center);
		AbilityInitials->SetVisibility(ESlateVisibility::HitTestInvisible);

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

void UMMOActionSlotWidget::SetCooldown(float Remaining, float Duration)
{
	const float Fraction = Duration > 0.0f ? FMath::Clamp(Remaining / Duration, 0.0f, 1.0f) : 0.0f;
	CooldownShade->SetHeightOverride((Size - 4.0f) * Fraction);
	// the short global cooldown only shows the sweep; real cooldowns also count down
	const bool bShowNumber = Remaining > 0.0f && Duration > 2.0f;
	CooldownText->SetText(bShowNumber ? FText::FromString(Remaining >= 60.0f ? FString::Printf(TEXT("%dm"), FMath::CeilToInt(Remaining / 60.0f)) : FString::FromInt(FMath::CeilToInt(Remaining))) : FText::GetEmpty());
}

void UMMOActionSlotWidget::Refresh()
{
	AMMOCharacter* Owner = Character.Get();
	if (!Owner || !ItemSlot)
	{
		return;
	}

	const FMMOActionSlot& Action = Owner->GetActionBar()->GetSlot(Index);
	const bool bChanged = Action.Id != ShownId || static_cast<uint8>(Action.Type) != ShownType;

	if (Action.Type == EMMOActionType::Ability)
	{
		UMMOAbilityDefinition* Ability = UMMOAbilityDefinition::FindById(Action.Id);
		if (bChanged)
		{
			ShownId = Action.Id;
			ShownType = static_cast<uint8>(Action.Type);
			ShownCount = -1;
			ItemSlot->SetStack(FMMOItemStack());
			ItemSlot->SetRenderOpacity(1.0f);
			UTexture2D* Texture = Ability ? Ability->Icon.Get() : nullptr;
			if (Texture)
			{
				AbilityIcon->SetBrushFromTexture(Texture);
			}
			AbilityIcon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			AbilityInitials->SetText(FText::FromString(Ability && !Texture ? MMOAbilityUI::Initials(Ability->DisplayName) : FString()));
			if (Ability)
			{
				if (!AbilityTooltip)
				{
					AbilityTooltip = CreateWidget<UMMOAbilityTooltipWidget>(this, UMMOAbilityTooltipWidget::StaticClass());
				}
				AbilityTooltip->SetAbility(Ability, true);
				SetToolTip(AbilityTooltip);
			}
		}

		// red when the selected enemy is out of reach
		const UMMOAbilityComponent* Abilities = Owner->GetAbilities();
		const bool bOutOfRange = Ability && Ability->TargetType == EMMOAbilityTarget::Enemy && Owner->GetCombat()->GetCurrentTarget() && !Abilities->IsTargetInRange(Ability);
		AbilityIcon->SetColorAndOpacity(bOutOfRange ? FLinearColor(1.0f, 0.3f, 0.3f) : FLinearColor::White);

		float Remaining = 0.0f, Duration = 0.0f;
		Abilities->GetCooldown(Ability, Remaining, Duration);
		SetCooldown(Remaining, Duration);
		return;
	}

	// items (or empty)
	UMMOItemDefinition* Item = Action.Type == EMMOActionType::Item ? UMMOItemDefinition::FindById(Action.Id) : nullptr;
	const int32 Count = Item ? Owner->GetInventory()->CountItem(Item) : 0;
	if (bChanged || Count != ShownCount)
	{
		ShownId = Action.Id;
		ShownType = static_cast<uint8>(Action.Type);
		ShownCount = Count;
		AbilityIcon->SetVisibility(ESlateVisibility::Hidden);
		AbilityInitials->SetText(FText::GetEmpty());
		SetToolTip(nullptr);
		ItemSlot->SetStack(Item ? FMMOItemStack::Make(Item, FMath::Max(Count, 1)) : FMMOItemStack());
		ItemSlot->SetRenderOpacity(Item && Count == 0 ? 0.35f : 1.0f);
	}

	const FName CooldownKey = Item ? Item->GetCooldownKey() : NAME_None;
	SetCooldown(Item ? Owner->GetCooldowns()->GetRemaining(CooldownKey) : 0.0f, Item ? Owner->GetCooldowns()->GetDuration(CooldownKey) : 0.0f);
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
	AMMOCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return false;
	}

	if (const UMMOAbilityDragOperation* AbilityDrag = Cast<UMMOAbilityDragOperation>(InOperation))
	{
		Owner->GetActionBar()->SetSlot(Index, UMMOActionBarComponent::MakeAbility(AbilityDrag->AbilityId));
		return true;
	}

	const UMMOItemDragOperation* Operation = Cast<UMMOItemDragOperation>(InOperation);
	const UMMOItemSlotWidget* Source = Operation ? Operation->SourceSlot.Get() : nullptr;
	if (!Source || Source->GetStack().IsEmpty())
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

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOLootWindowWidget.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOLootContainerComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace MMOLootRow
{
	static const FLinearColor Idle(0.06f, 0.06f, 0.07f, 0.9f);
	static const FLinearColor Hover(0.16f, 0.13f, 0.08f, 0.95f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Row

void UMMOLootRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	Background = MMOUI::MakePanel(WidgetTree, MMOLootRow::Idle, 5.0f, FMargin(4.0f));
	WidgetTree->RootWidget = Background;

	UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Background->SetContent(Line);

	ItemSlot = WidgetTree->ConstructWidget<UMMOItemSlotWidget>(UMMOItemSlotWidget::StaticClass());
	ItemSlot->Configure(EMMOItemSlotContext::Display, INDEX_NONE, EMMOEquipmentSlot::None, FText::GetEmpty(), 40.0f);
	Line->AddChildToHorizontalBox(ItemSlot);

	NameText = MMOUI::MakeText(WidgetTree, TEXT(""), 13, FLinearColor::White, true);
	UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(NameText);
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NameSlot->SetVerticalAlignment(VAlign_Center);
	NameSlot->SetPadding(FMargin(8.0f, 0.0f));

	QuantityText = MMOUI::MakeText(WidgetTree, TEXT(""), 12, MMOUI::Colors::TextDim, true, ETextJustify::Right);
	UHorizontalBoxSlot* QuantitySlot = Line->AddChildToHorizontalBox(QuantityText);
	QuantitySlot->SetVerticalAlignment(VAlign_Center);
	QuantitySlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
}

void UMMOLootRowWidget::SetItem(const FMMOItemStack& Stack)
{
	bCurrency = false;
	InstanceId = Stack.InstanceId;
	ItemSlot->SetStack(Stack);
	NameText->SetText(Stack.Item->DisplayName);
	NameText->SetColorAndOpacity(FSlateColor(MMOItems::GetRarityColor(Stack.Item->Rarity)));
	QuantityText->SetText(FText::FromString(Stack.Quantity > 1 ? FString::Printf(TEXT("x%d"), Stack.Quantity) : FString()));
}

void UMMOLootRowWidget::SetCurrency(int32 Amount)
{
	bCurrency = true;
	InstanceId.Invalidate();
	ItemSlot->SetStack(FMMOItemStack());
	NameText->SetText(FText::FromString(MMOItems::FormatCurrency(Amount)));
	NameText->SetColorAndOpacity(FSlateColor(MMOUI::Colors::Gold));
	QuantityText->SetText(FText::GetEmpty());
}

FReply UMMOLootRowWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		OnClicked.ExecuteIfBound(this);
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UMMOLootRowWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	Background->SetBrush(MMOUI::RoundedBrush(MMOLootRow::Hover, 5.0f, MMOUI::Colors::Gold, 1.0f));
}

void UMMOLootRowWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	Background->SetBrush(MMOUI::RoundedBrush(MMOLootRow::Idle, 5.0f));
}

// ---------------------------------------------------------------------------------------------------------------------
// Window

void UMMOLootWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
}

void UMMOLootWindowWidget::BuildLayout()
{
	using namespace MMOUI;

	UVerticalBox* Content = nullptr;
	UButton* CloseButtonWidget = nullptr;
	UTextBlock* TitleWidget = nullptr;
	WidgetTree->RootWidget = MakeWindow(WidgetTree, TEXT("Loot"), 280.0f, Content, CloseButtonWidget, &TitleWidget);
	CloseButton = CloseButtonWidget;
	TitleText = TitleWidget;
	CloseButton->OnClicked.AddDynamic(this, &UMMOLootWindowWidget::HandleClose);

	Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Content->AddChildToVerticalBox(Rows);

	LootAllButton = MakeButton(WidgetTree, TEXT("Loot All"), 13);
	LootAllButton->OnClicked.AddDynamic(this, &UMMOLootWindowWidget::HandleLootAll);
	UVerticalBoxSlot* ButtonSlot = Content->AddChildToVerticalBox(LootAllButton);
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	ButtonSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
}

void UMMOLootWindowWidget::Open(AMMOCharacter* InCharacter, UMMOLootContainerComponent* InContainer)
{
	if (UMMOLootContainerComponent* Old = Container.Get())
	{
		Old->OnLootChanged.RemoveAll(this);
	}

	Character = InCharacter;
	Container = InContainer;
	if (InContainer)
	{
		InContainer->OnLootChanged.AddDynamic(this, &UMMOLootWindowWidget::Refresh);
	}
	Refresh();
}

void UMMOLootWindowWidget::Close()
{
	if (UMMOLootContainerComponent* Old = Container.Get())
	{
		Old->OnLootChanged.RemoveAll(this);
	}
	Container.Reset();
	if (Rows)
	{
		Rows->ClearChildren();
	}
}

int32 UMMOLootWindowWidget::GetRowCount() const
{
	return Rows ? Rows->GetChildrenCount() : 0;
}

void UMMOLootWindowWidget::Refresh()
{
	if (!Rows)
	{
		return;
	}

	Rows->ClearChildren();

	const UMMOLootContainerComponent* Loot = Container.Get();
	if (!Loot)
	{
		return;
	}

	if (TitleText)
	{
		TitleText->SetText(Loot->ContainerName.IsEmpty() ? NSLOCTEXT("MMOLoot", "Loot", "Loot") : Loot->ContainerName);
	}

	auto AddRow = [this]()
	{
		UMMOLootRowWidget* Row = WidgetTree->ConstructWidget<UMMOLootRowWidget>(UMMOLootRowWidget::StaticClass());
		Row->OnClicked.BindUObject(this, &UMMOLootWindowWidget::HandleRowClicked);
		Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));
		return Row;
	};

	if (Loot->GetCurrency() > 0)
	{
		AddRow()->SetCurrency(Loot->GetCurrency());
	}
	for (const FMMOItemStack& Stack : Loot->GetItems())
	{
		AddRow()->SetItem(Stack);
	}
}

void UMMOLootWindowWidget::HandleRowClicked(UMMOLootRowWidget* Row)
{
	AMMOCharacter* Owner = Character.Get();
	UMMOLootContainerComponent* Loot = Container.Get();
	if (!Owner || !Loot || !Row)
	{
		return;
	}

	// rows refer to stacks by id, so a stale or repeated click can never take something twice
	if (Row->IsCurrency())
	{
		Owner->LootCurrency(Loot);
	}
	else
	{
		Owner->LootItem(Loot, Row->GetInstanceId());
	}
}

void UMMOLootWindowWidget::HandleLootAll()
{
	AMMOCharacter* Owner = Character.Get();
	UMMOLootContainerComponent* Loot = Container.Get();
	if (Owner && Loot && Loot->HasLoot())
	{
		Owner->LootAll(Loot);
	}
}

void UMMOLootWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

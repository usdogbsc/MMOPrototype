// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOInventoryWindowWidget.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UMMOInventoryWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
}

void UMMOInventoryWindowWidget::BuildLayout()
{
	using namespace MMOUI;

	UVerticalBox* Content = nullptr;
	UButton* CloseButtonWidget = nullptr;
	UTextBlock* TitleWidget = nullptr;
	WidgetTree->RootWidget = MakeWindow(WidgetTree, TEXT("Backpack"), 5 * 54.0f + 24.0f, Content, CloseButtonWidget, &TitleWidget);
	CloseButton = CloseButtonWidget;
	CloseButton->OnClicked.AddDynamic(this, &UMMOInventoryWindowWidget::HandleClose);

	Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
	Grid->SetSlotPadding(FMargin(2.0f));
	Content->AddChildToVerticalBox(Grid)->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	FreeSlotsText = MakeText(WidgetTree, TEXT(""), 11, Colors::TextDim);
	CurrencyText = MakeText(WidgetTree, TEXT(""), 13, Colors::Gold, true, ETextJustify::Right);
	UHorizontalBoxSlot* FreeSlot = Footer->AddChildToHorizontalBox(FreeSlotsText);
	FreeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	FreeSlot->SetVerticalAlignment(VAlign_Center);
	Footer->AddChildToHorizontalBox(CurrencyText)->SetVerticalAlignment(VAlign_Center);
	Content->AddChildToVerticalBox(Footer)->SetPadding(FMargin(2.0f, 8.0f, 2.0f, 0.0f));
}

void UMMOInventoryWindowWidget::Init(AMMOCharacter* InCharacter)
{
	if (Character.Get() == InCharacter)
	{
		return;
	}

	if (AMMOCharacter* Old = Character.Get())
	{
		Old->GetInventory()->OnInventoryChanged.RemoveAll(this);
		Old->GetEquipment()->OnEquipmentChanged.RemoveAll(this);
	}

	Character = InCharacter;
	if (InCharacter)
	{
		InCharacter->GetInventory()->OnInventoryChanged.AddDynamic(this, &UMMOInventoryWindowWidget::Refresh);
		InCharacter->GetEquipment()->OnEquipmentChanged.AddDynamic(this, &UMMOInventoryWindowWidget::Refresh);
	}
	Refresh();
}

void UMMOInventoryWindowWidget::RebuildSlots(int32 Count)
{
	if (!Grid || SlotWidgets.Num() == Count)
	{
		return;
	}

	Grid->ClearChildren();
	SlotWidgets.Reset();
	for (int32 i = 0; i < Count; ++i)
	{
		UMMOItemSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UMMOItemSlotWidget>(UMMOItemSlotWidget::StaticClass());
		SlotWidget->Configure(EMMOItemSlotContext::Inventory, i);
		SlotWidget->OnRightClicked.BindUObject(this, &UMMOInventoryWindowWidget::HandleSlotUse);
		SlotWidget->OnDoubleClicked.BindUObject(this, &UMMOInventoryWindowWidget::HandleSlotUse);
		SlotWidget->OnDropped.BindUObject(this, &UMMOInventoryWindowWidget::HandleSlotDropped);
		Grid->AddChildToUniformGrid(SlotWidget, i / Columns, i % Columns);
		SlotWidgets.Add(SlotWidget);
	}
}

void UMMOInventoryWindowWidget::Refresh()
{
	const AMMOCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return;
	}

	const UMMOInventoryComponent* Inventory = Owner->GetInventory();
	const UMMOEquipmentComponent* Equipment = Owner->GetEquipment();
	RebuildSlots(Inventory->GetSlots().Num());

	for (int32 i = 0; i < SlotWidgets.Num(); ++i)
	{
		const FMMOItemStack& Stack = Inventory->GetSlot(i);
		const FMMOItemStack* Worn = (!Stack.IsEmpty() && Stack.Item->IsEquippable()) ? &Equipment->GetEquipped(Stack.Item->EquipmentSlot) : nullptr;
		SlotWidgets[i]->SetStack(Stack, Worn);
	}

	if (CurrencyText)
	{
		CurrencyText->SetText(FText::FromString(MMOItems::FormatCurrency(Inventory->GetCurrency())));
	}
	if (FreeSlotsText)
	{
		FreeSlotsText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d free"), Inventory->GetFreeSlotCount(), Inventory->GetSlots().Num())));
	}
}

void UMMOInventoryWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

void UMMOInventoryWindowWidget::HandleSlotUse(UMMOItemSlotWidget* SlotWidget)
{
	AMMOCharacter* Owner = Character.Get();
	if (Owner && SlotWidget && !SlotWidget->GetStack().IsEmpty())
	{
		Owner->UseOrEquipInventorySlot(SlotWidget->GetSlotIndex());
	}
}

void UMMOInventoryWindowWidget::HandleSlotDropped(UMMOItemSlotWidget* Target, UMMOItemSlotWidget* Source)
{
	AMMOCharacter* Owner = Character.Get();
	if (!Owner || !Target || !Source)
	{
		return;
	}

	if (Source->GetContext() == EMMOItemSlotContext::Inventory)
	{
		Owner->MoveInventorySlot(Source->GetSlotIndex(), Target->GetSlotIndex());
	}
	else if (Source->GetContext() == EMMOItemSlotContext::Equipment)
	{
		// dropping worn gear onto the bag: into that slot if it is empty, otherwise anywhere free
		Owner->UnequipSlot(Source->GetEquipSlot(), Target->GetSlotIndex());
	}
}

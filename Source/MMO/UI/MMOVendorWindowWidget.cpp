// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOVendorWindowWidget.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMONPCPlateWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "NPC/MMONPC.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOVendor.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace MMOVendorUI
{
	/** Icon, name and price, with a button on the right */
	static UHorizontalBox* MakeRow(UWidgetTree* Tree, const FMMOItemStack& Stack, int32 Price, bool bAffordable, const FString& ButtonLabel, TFunction<void()> OnClick)
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UMMOItemSlotWidget* Icon = Tree->ConstructWidget<UMMOItemSlotWidget>(UMMOItemSlotWidget::StaticClass());
		Icon->Configure(EMMOItemSlotContext::Display, INDEX_NONE, EMMOEquipmentSlot::None, FText::GetEmpty(), 40.0f);
		Icon->SetStack(Stack);
		Row->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));

		UVerticalBox* Text = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		const FString Name = Stack.Quantity > 1 ? FString::Printf(TEXT("%s x%d"), *Stack.Item->DisplayName.ToString(), Stack.Quantity) : Stack.Item->DisplayName.ToString();
		Text->AddChildToVerticalBox(MMOUI::MakeText(Tree, Name, 13, MMOItems::GetRarityColor(Stack.Item->Rarity), true));
		Text->AddChildToVerticalBox(MMOUI::MakeText(Tree, MMOItems::FormatCurrency(Price), 12, bAffordable ? MMOUI::Colors::Gold : MMOUI::Colors::Error));
		UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);

		USizeBox* ButtonBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ButtonBox->SetWidthOverride(92.0f);
		UMMOTextButtonWidget* Button = Tree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
		Button->Setup(ButtonLabel, bAffordable ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.6f), 12);
		Button->OnClickedNative.BindLambda(MoveTemp(OnClick));
		ButtonBox->AddChild(Button);
		Row->AddChildToHorizontalBox(ButtonBox)->SetVerticalAlignment(VAlign_Center);
		return Row;
	}
}

void UMMOVendorWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		UTextBlock* Title = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT("Merchant"), 360.0f, Content, CloseWidget, &Title);
		CloseButton = CloseWidget;
		TitleText = Title;
		CloseButton->OnClicked.AddDynamic(this, &UMMOVendorWindowWidget::HandleClose);

		StockList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(StockList);

		BuybackList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(BuybackList)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));

		UTextBlock* Hint = MMOUI::MakeText(WidgetTree, TEXT("Right-click items in your backpack to sell them."), 11, MMOUI::Colors::TextDim);
		Content->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
		CurrencyText = MMOUI::MakeText(WidgetTree, TEXT(""), 13, MMOUI::Colors::Gold, true, ETextJustify::Right);
		Content->AddChildToVerticalBox(CurrencyText)->SetHorizontalAlignment(HAlign_Right);
	}
}

void UMMOVendorWindowWidget::Open(AMMOCharacter* InCharacter, AMMONPC* InVendor)
{
	Unbind();
	Character = InCharacter;
	Vendor = InVendor;
	if (InCharacter)
	{
		TradeHandle = InCharacter->OnTradeChanged.AddUObject(this, &UMMOVendorWindowWidget::Refresh);
		InCharacter->GetInventory()->OnInventoryChanged.AddDynamic(this, &UMMOVendorWindowWidget::Refresh);
	}
	if (TitleText && InVendor)
	{
		TitleText->SetText(InVendor->DisplayName);
	}
	Refresh();
}

void UMMOVendorWindowWidget::Close()
{
	Unbind();
	Vendor.Reset();
}

void UMMOVendorWindowWidget::Unbind()
{
	if (AMMOCharacter* Owner = Character.Get())
	{
		Owner->OnTradeChanged.Remove(TradeHandle);
		Owner->GetInventory()->OnInventoryChanged.RemoveAll(this);
	}
	TradeHandle.Reset();
}

void UMMOVendorWindowWidget::Refresh()
{
	AMMOCharacter* Owner = Character.Get();
	const AMMONPC* Merchant = Vendor.Get();
	if (!StockList || !Owner || !Merchant)
	{
		return;
	}
	const int32 Money = Owner->GetInventory()->GetCurrency();

	StockList->ClearChildren();
	for (int32 Index = 0; Index < Merchant->VendorStock.Num(); ++Index)
	{
		const FMMOVendorEntry& Entry = Merchant->VendorStock[Index];
		if (!Entry.Item)
		{
			continue;
		}
		TWeakObjectPtr<AMMOCharacter> WeakOwner = Owner;
		UHorizontalBox* Row = MMOVendorUI::MakeRow(WidgetTree, FMMOItemStack::Make(Entry.Item, 1), Entry.GetPrice(), Money >= Entry.GetPrice(), TEXT("Buy"),
			[WeakOwner, Index]() { if (WeakOwner.IsValid()) { WeakOwner->BuyFromVendor(Index); } });
		StockList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 3.0f));
	}

	BuybackList->ClearChildren();
	const TArray<FMMOItemStack>& Sold = Owner->GetBuyback();
	if (Sold.Num() > 0)
	{
		BuybackList->AddChildToVerticalBox(MMOUI::MakeText(WidgetTree, TEXT("Buyback"), 13, MMOUI::Colors::Gold, true));
		for (int32 Index = 0; Index < Sold.Num(); ++Index)
		{
			const int32 Price = MMOVendor::GetSellPrice(Sold[Index]);
			TWeakObjectPtr<AMMOCharacter> WeakOwner = Owner;
			UHorizontalBox* Row = MMOVendorUI::MakeRow(WidgetTree, Sold[Index], Price, Money >= Price, TEXT("Buy back"),
				[WeakOwner, Index]() { if (WeakOwner.IsValid()) { WeakOwner->BuybackItem(Index); } });
			BuybackList->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));
		}
	}

	CurrencyText->SetText(FText::FromString(FString::Printf(TEXT("Your money: %s"), *MMOItems::FormatCurrency(Money))));
}

void UMMOVendorWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

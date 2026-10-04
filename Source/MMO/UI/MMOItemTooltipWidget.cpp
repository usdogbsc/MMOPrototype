// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOItemTooltipWidget.h"
#include "UI/MMOUIStyle.h"
#include "Items/MMOItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UMMOItemTooltipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Root->SetMaxDesiredWidth(300.0f);
		WidgetTree->RootWidget = Root;

		UBorder* Panel = MMOUI::MakePanel(WidgetTree, FLinearColor(0.01f, 0.012f, 0.018f, 0.97f), 6.0f, FMargin(10.0f, 8.0f), MMOUI::Colors::PanelOutline, 1.0f);
		Root->AddChild(Panel);

		Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Panel->SetContent(Content);
	}
}

void UMMOItemTooltipWidget::AddLine(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold, float TopPadding)
{
	if (!Content || Text.IsEmpty())
	{
		return;
	}

	UTextBlock* Line = MMOUI::MakeText(WidgetTree, Text, Size, Color, bBold);
	// fixed wrap width so the tooltip measures correctly on its first frame
	Line->SetWrapTextAt(276.0f);
	Content->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
}

TArray<FString> UMMOItemTooltipWidget::GetStatLines(const UMMOItemDefinition* Item)
{
	TArray<FString> Lines;
	if (!Item)
	{
		return Lines;
	}

	if (Item->IsWeapon())
	{
		Lines.Add(FString::Printf(TEXT("Damage: %d - %d"), FMath::RoundToInt(Item->WeaponDamageMin), FMath::RoundToInt(Item->WeaponDamageMax)));
	}
	if (Item->Stats.Armor > 0.0f)
	{
		Lines.Add(FString::Printf(TEXT("+%d Armor"), FMath::RoundToInt(Item->Stats.Armor)));
	}
	if (Item->Stats.MaxHealth > 0.0f)
	{
		Lines.Add(FString::Printf(TEXT("+%d Max Health"), FMath::RoundToInt(Item->Stats.MaxHealth)));
	}
	if (Item->Stats.AttackDamage > 0.0f)
	{
		Lines.Add(FString::Printf(TEXT("+%d Attack Damage"), FMath::RoundToInt(Item->Stats.AttackDamage)));
	}
	return Lines;
}

void UMMOItemTooltipWidget::SetItem(const FMMOItemStack& Stack, const FMMOItemStack* Compare)
{
	if (!Content)
	{
		return;
	}

	Content->ClearChildren();
	if (Stack.IsEmpty())
	{
		return;
	}

	const UMMOItemDefinition* Item = Stack.Item;
	const FLinearColor RarityColor = MMOItems::GetRarityColor(Item->Rarity);

	AddLine(Item->DisplayName.ToString(), 16, RarityColor, true);
	AddLine(MMOItems::GetRarityText(Item->Rarity).ToString(), 11, RarityColor);
	AddLine(Item->CategoryText.ToString(), 12, MMOUI::Colors::TextDim);
	if (Item->IsEquippable())
	{
		AddLine(FString::Printf(TEXT("Equip: %s"), *MMOItems::GetSlotText(Item->EquipmentSlot).ToString()), 12, MMOUI::Colors::TextDim);
	}

	const TArray<FString> Stats = GetStatLines(Item);
	for (int32 i = 0; i < Stats.Num(); ++i)
	{
		AddLine(Stats[i], 13, FLinearColor::White, false, i == 0 ? 6.0f : 0.0f);
	}

	// consumables: what using it does (green, like most MMOs)
	if (Item->IsUsable())
	{
		const FLinearColor UseColor(0.3f, 0.95f, 0.35f);
		if (Item->HealAmount > 0.0f)
		{
			AddLine(FString::Printf(TEXT("Use: Restores %d health."), FMath::RoundToInt(Item->HealAmount)), 12, UseColor, false, 6.0f);
		}
		if (Item->HealOverTime > 0.0f)
		{
			AddLine(FString::Printf(TEXT("Use: Restores %d health over %d sec. Must remain out of combat; taking damage stops it."),
				FMath::RoundToInt(Item->HealOverTime), FMath::RoundToInt(Item->EffectDuration)), 12, UseColor, false, 6.0f);
		}
		if (Item->Cooldown > 0.0f)
		{
			const FString Time = Item->Cooldown >= 60.0f ? FString::Printf(TEXT("%d min"), FMath::RoundToInt(Item->Cooldown / 60.0f)) : FString::Printf(TEXT("%d sec"), FMath::RoundToInt(Item->Cooldown));
			AddLine(FString::Printf(TEXT("Cooldown: %s"), *Time), 11, MMOUI::Colors::TextDim);
		}
	}

	AddLine(Item->Description.ToString(), 12, FLinearColor(0.88f, 0.86f, 0.8f), false, 6.0f);
	AddLine(Item->FlavorText.ToString(), 12, FLinearColor(0.95f, 0.78f, 0.45f), false, 4.0f);

	if (Item->IsStackable())
	{
		AddLine(FString::Printf(TEXT("Stack: %d / %d"), Stack.Quantity, Item->MaxStackSize), 11, MMOUI::Colors::TextDim, false, 6.0f);
	}
	if (Item->SellValue > 0)
	{
		AddLine(FString::Printf(TEXT("Sell value: %s"), *MMOItems::FormatCurrency(Item->SellValue)), 11, MMOUI::Colors::TextDim, false, Item->IsStackable() ? 0.0f : 6.0f);
	}

	// simple comparison with whatever is worn in the same slot
	if (Item->IsEquippable() && Compare && !Compare->IsEmpty() && Compare->InstanceId != Stack.InstanceId)
	{
		AddLine(TEXT("Currently equipped:"), 11, MMOUI::Colors::TextDim, false, 8.0f);
		AddLine(Compare->Item->DisplayName.ToString(), 13, MMOItems::GetRarityColor(Compare->Item->Rarity), true);
		for (const FString& Line : GetStatLines(Compare->Item))
		{
			AddLine(Line, 12, MMOUI::Colors::TextDim);
		}
	}
}

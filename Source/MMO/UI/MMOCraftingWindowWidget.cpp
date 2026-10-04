// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOCraftingWindowWidget.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMONPCPlateWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Professions/MMOProfessionComponent.h"
#include "Professions/MMORecipeDefinition.h"
#include "World/MMOCraftingStation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UMMOCraftingWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		UTextBlock* Title = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT("Crafting"), 430.0f, Content, CloseWidget, &Title);
		TitleText = Title;
		CloseButton = CloseWidget;
		CloseButton->OnClicked.AddDynamic(this, &UMMOCraftingWindowWidget::HandleClose);

		SkillText = MMOUI::MakeText(WidgetTree, TEXT(""), 13, FLinearColor(0.45f, 0.75f, 1.0f), true);
		Content->AddChildToVerticalBox(SkillText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(List);
		UTextBlock* Hint = MMOUI::MakeText(WidgetTree, TEXT("Orange and yellow recipes raise your skill; grey ones no longer do. Moving stops crafting."), 11, MMOUI::Colors::TextDim);
		Hint->SetWrapTextAt(390.0f);
		Content->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
}

void UMMOCraftingWindowWidget::Open(AMMOCharacter* InCharacter, AMMOCraftingStation* InStation)
{
	Unbind();
	Character = InCharacter;
	Station = InStation;
	if (InCharacter)
	{
		InCharacter->GetInventory()->OnInventoryChanged.AddDynamic(this, &UMMOCraftingWindowWidget::Refresh);
		InCharacter->GetProfessions()->OnSkillChanged.AddDynamic(this, &UMMOCraftingWindowWidget::HandleSkillChanged);
		ActivityHandle = InCharacter->GetProfessions()->OnActivityChanged.AddUObject(this, &UMMOCraftingWindowWidget::Refresh);
	}
	if (TitleText && InStation)
	{
		TitleText->SetText(InStation->DisplayName);
	}
	Refresh();
}

void UMMOCraftingWindowWidget::Close()
{
	Unbind();
	Station.Reset();
}

void UMMOCraftingWindowWidget::Unbind()
{
	if (AMMOCharacter* Owner = Character.Get())
	{
		Owner->GetInventory()->OnInventoryChanged.RemoveAll(this);
		Owner->GetProfessions()->OnSkillChanged.RemoveAll(this);
		Owner->GetProfessions()->OnActivityChanged.Remove(ActivityHandle);
	}
	ActivityHandle.Reset();
}

void UMMOCraftingWindowWidget::HandleSkillChanged(EMMOProfession Profession, int32 NewSkill)
{
	Refresh();
}

void UMMOCraftingWindowWidget::Refresh()
{
	AMMOCharacter* Owner = Character.Get();
	const AMMOCraftingStation* Bench = Station.Get();
	if (!List || !Owner || !Bench)
	{
		return;
	}
	UMMOProfessionComponent* Professions = Owner->GetProfessions();
	const UMMOInventoryComponent* Inventory = Owner->GetInventory();
	const int32 Skill = Professions->GetSkill(Bench->Profession);
	SkillText->SetText(FText::FromString(FString::Printf(TEXT("%s  %d / %d"), *MMOProfessions::GetName(Bench->Profession).ToString(), Skill, MMOProfessions::MaxSkill)));

	List->ClearChildren();
	for (UMMORecipeDefinition* Recipe : Bench->Recipes)
	{
		if (!Recipe || !Recipe->Output)
		{
			continue;
		}
		const EMMOSkillDifficulty Difficulty = MMOProfessions::GetDifficulty(Skill, Recipe->RequiredSkill);
		const int32 Craftable = Recipe->GetMaxCraftable(Inventory);

		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UMMOItemSlotWidget* Icon = WidgetTree->ConstructWidget<UMMOItemSlotWidget>(UMMOItemSlotWidget::StaticClass());
		Icon->Configure(EMMOItemSlotContext::Display, INDEX_NONE, EMMOEquipmentSlot::None, FText::GetEmpty(), 42.0f);
		Icon->SetStack(FMMOItemStack::Make(Recipe->Output, Recipe->OutputQuantity));
		UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(Icon);
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Top);

		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		const FString Name = Craftable > 0 ? FString::Printf(TEXT("%s  [%d]"), *Recipe->Output->DisplayName.ToString(), Craftable) : Recipe->Output->DisplayName.ToString();
		Text->AddChildToVerticalBox(MMOUI::MakeText(WidgetTree, Name, 13, MMOProfessions::GetDifficultyColor(Difficulty), true));
		if (Difficulty == EMMOSkillDifficulty::TooHard)
		{
			Text->AddChildToVerticalBox(MMOUI::MakeText(WidgetTree, FString::Printf(TEXT("Requires %s %d"), *MMOProfessions::GetName(Recipe->Profession).ToString(), Recipe->RequiredSkill), 11, MMOUI::Colors::Error));
		}
		TArray<FString> Parts;
		for (const FMMOIngredient& Ingredient : Recipe->Ingredients)
		{
			if (Ingredient.Item)
			{
				Parts.Add(FString::Printf(TEXT("%s %d/%d"), *Ingredient.Item->DisplayName.ToString(), Inventory->CountItem(Ingredient.Item), Ingredient.Quantity));
			}
		}
		UTextBlock* Reagents = MMOUI::MakeText(WidgetTree, FString::Join(Parts, TEXT(",  ")), 11, Craftable > 0 ? FLinearColor(0.85f, 0.85f, 0.8f) : FLinearColor(0.9f, 0.45f, 0.4f));
		Reagents->SetWrapTextAt(210.0f);
		Text->AddChildToVerticalBox(Reagents);
		UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);

		const bool bCanCraft = Professions->CanCraft(Recipe) == EMMOProfessionResult::Started && !Professions->IsBusy();
		UVerticalBox* Buttons = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TWeakObjectPtr<AMMOCharacter> WeakOwner = Owner;
		TWeakObjectPtr<AMMOCraftingStation> WeakStation = Station;
		TWeakObjectPtr<UMMORecipeDefinition> WeakRecipe = Recipe;
		for (const bool bAll : { false, true })
		{
			USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			ButtonBox->SetWidthOverride(84.0f);
			UMMOTextButtonWidget* Button = WidgetTree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
			Button->Setup(bAll ? TEXT("Craft All") : TEXT("Craft"), bCanCraft ? FLinearColor::White : FLinearColor(0.55f, 0.55f, 0.55f), 12);
			Button->OnClickedNative.BindLambda([WeakOwner, WeakStation, WeakRecipe, bAll]()
			{
				if (WeakOwner.IsValid() && WeakRecipe.IsValid())
				{
					WeakOwner->GetProfessions()->StartCraft(WeakRecipe.Get(), bAll ? 1000 : 1, WeakStation.Get());
				}
			});
			ButtonBox->AddChild(Button);
			Buttons->AddChildToVerticalBox(ButtonBox)->SetPadding(FMargin(0.0f, 1.0f));
		}
		Row->AddChildToHorizontalBox(Buttons)->SetVerticalAlignment(VAlign_Center);

		List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f));
	}
}

void UMMOCraftingWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

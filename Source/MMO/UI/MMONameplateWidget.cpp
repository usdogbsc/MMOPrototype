// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMONameplateWidget.h"
#include "UI/MMOUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UMMONameplateWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMMONameplateWidget::BuildDefaultLayout()
{
	using namespace MMOUI;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Root;

	NameText = MakeText(WidgetTree, TEXT(""), 13, Colors::Hostile, true, ETextJustify::Center);
	Root->AddChildToVerticalBox(NameText)->SetHorizontalAlignment(HAlign_Center);

	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(96.0f);
	BarBox->SetHeightOverride(8.0f);
	HealthBar = MakeBar(WidgetTree, Colors::EnemyHealth, Colors::BarBack, 2.0f);
	BarBox->AddChild(HealthBar);
	HealthRow = BarBox;

	UVerticalBoxSlot* BarSlot = Root->AddChildToVerticalBox(BarBox);
	BarSlot->SetHorizontalAlignment(HAlign_Center);
	BarSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 0.0f));
}

void UMMONameplateWidget::SetNameplateState(const FText& Name, int32 Level, float HealthPercent, bool bTargeted, bool bDead, bool bInCombat)
{
	if (NameText)
	{
		const FString Label = bTargeted ? FString::Printf(TEXT("> %s <"), *Name.ToString()) : Name.ToString();
		if (Label != CachedLabel)
		{
			CachedLabel = Label;
			NameText->SetText(FText::FromString(Label));
		}

		const FLinearColor Color = bDead ? MMOUI::Colors::Dead : (bTargeted ? MMOUI::Colors::Gold : MMOUI::Colors::Hostile);
		NameText->SetColorAndOpacity(FSlateColor(Color));
	}

	if (HealthBar)
	{
		HealthBar->SetPercent(HealthPercent);
	}

	if (HealthRow)
	{
		// health bars appear once a fight starts or when selected, keeping the idle world uncluttered
		const bool bShowBar = !bDead && (bTargeted || bInCombat || HealthPercent < 1.0f);
		HealthRow->SetVisibility(bShowBar ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

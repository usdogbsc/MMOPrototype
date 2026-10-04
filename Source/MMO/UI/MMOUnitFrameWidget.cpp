// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOUnitFrameWidget.h"
#include "UI/MMOUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UMMOUnitFrameWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}

	SetHealthColor(HealthColor);
}

void UMMOUnitFrameWidget::BuildDefaultLayout()
{
	using namespace MMOUI;

	USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Root->SetWidthOverride(290.0f);
	WidgetTree->RootWidget = Root;

	UBorder* Panel = MakePanel(WidgetTree, Colors::Panel, 6.0f, FMargin(10.0f, 7.0f), Colors::PanelOutline, 1.0f);
	Root->AddChild(Panel);

	UVerticalBox* Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Panel->SetContent(Rows);

	// name + level
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	NameText = MakeText(WidgetTree, TEXT(""), 15, FLinearColor::White, true);
	LevelText = MakeText(WidgetTree, TEXT(""), 13, Colors::Gold, true, ETextJustify::Right);
	UHorizontalBoxSlot* NameSlot = Header->AddChildToHorizontalBox(NameText);
	NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	NameSlot->SetVerticalAlignment(VAlign_Center);
	Header->AddChildToHorizontalBox(LevelText)->SetVerticalAlignment(VAlign_Center);
	Rows->AddChildToVerticalBox(Header)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));

	// health: chip bar behind, health bar on top, text centered
	USizeBox* HealthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	HealthBox->SetHeightOverride(22.0f);
	UOverlay* HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	HealthBox->AddChild(HealthOverlay);

	HealthChipBar = MakeBar(WidgetTree, Colors::HealthChip, Colors::BarBack);
	HealthBar = MakeBar(WidgetTree, HealthColor, FLinearColor::Transparent);
	HealthText = MakeText(WidgetTree, TEXT(""), 12, FLinearColor::White, true, ETextJustify::Center);

	for (UWidget* Child : { static_cast<UWidget*>(HealthChipBar), static_cast<UWidget*>(HealthBar) })
	{
		UOverlaySlot* BarSlot = HealthOverlay->AddChildToOverlay(Child);
		BarSlot->SetHorizontalAlignment(HAlign_Fill);
		BarSlot->SetVerticalAlignment(VAlign_Fill);
	}
	UOverlaySlot* HealthTextSlot = HealthOverlay->AddChildToOverlay(HealthText);
	HealthTextSlot->SetHorizontalAlignment(HAlign_Center);
	HealthTextSlot->SetVerticalAlignment(VAlign_Center);
	Rows->AddChildToVerticalBox(HealthBox);

	// XP (player only)
	USizeBox* XPBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	XPBox->SetHeightOverride(15.0f);
	UOverlay* XPOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	XPBox->AddChild(XPOverlay);
	XPBar = MakeBar(WidgetTree, Colors::XP, Colors::BarBack, 2.0f);
	XPText = MakeText(WidgetTree, TEXT(""), 10, FLinearColor::White, true, ETextJustify::Center);
	UOverlaySlot* XPBarSlot = XPOverlay->AddChildToOverlay(XPBar);
	XPBarSlot->SetHorizontalAlignment(HAlign_Fill);
	XPBarSlot->SetVerticalAlignment(VAlign_Fill);
	UOverlaySlot* XPTextSlot = XPOverlay->AddChildToOverlay(XPText);
	XPTextSlot->SetHorizontalAlignment(HAlign_Center);
	XPTextSlot->SetVerticalAlignment(VAlign_Center);
	XPRow = XPBox;
	Rows->AddChildToVerticalBox(XPBox)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
}

void UMMOUnitFrameWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// the chip bar holds briefly after a hit, then drains to the real value
	if (ChipPercent > HealthPercent)
	{
		ChipHoldTime -= InDeltaTime;
		if (ChipHoldTime <= 0.0f)
		{
			ChipPercent = FMath::Max(HealthPercent, ChipPercent - InDeltaTime * 0.8f);
		}
	}
	else
	{
		ChipPercent = HealthPercent;
	}

	if (HealthChipBar)
	{
		HealthChipBar->SetPercent(ChipPercent);
	}
}

void UMMOUnitFrameWidget::SetUnitName(const FText& Name, const FLinearColor& Color)
{
	if (NameText)
	{
		NameText->SetText(Name);
		NameText->SetColorAndOpacity(FSlateColor(Color));
	}
}

void UMMOUnitFrameWidget::SetLevel(int32 Level)
{
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(FString::Printf(TEXT("Lv %d"), Level)));
	}
}

void UMMOUnitFrameWidget::SetHealth(float Current, float Max, bool bDead)
{
	const float NewPercent = Max > 0.0f ? FMath::Clamp(Current / Max, 0.0f, 1.0f) : 0.0f;
	if (NewPercent < HealthPercent)
	{
		ChipHoldTime = 0.35f;
	}
	HealthPercent = NewPercent;

	if (HealthBar)
	{
		HealthBar->SetPercent(HealthPercent);
	}
	if (HealthText)
	{
		HealthText->SetText(bDead
			? NSLOCTEXT("MMOHUD", "Dead", "Dead")
			: FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Current), FMath::RoundToInt(Max))));
	}
}

void UMMOUnitFrameWidget::SetXP(int32 Current, int32 Required, bool bMaxLevel)
{
	if (XPBar)
	{
		XPBar->SetPercent(bMaxLevel ? 1.0f : static_cast<float>(Current) / FMath::Max(1, Required));
	}
	if (XPText)
	{
		XPText->SetText(bMaxLevel
			? NSLOCTEXT("MMOHUD", "MaxLevel", "Max level")
			: FText::FromString(FString::Printf(TEXT("XP %d / %d"), Current, Required)));
	}
}

void UMMOUnitFrameWidget::SetShowXP(bool bShow)
{
	if (XPRow)
	{
		XPRow->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMMOUnitFrameWidget::ResetHealthChip()
{
	ChipPercent = HealthPercent;
	ChipHoldTime = 0.0f;
}

void UMMOUnitFrameWidget::SetHealthColor(const FLinearColor& Color)
{
	HealthColor = Color;
	if (HealthBar)
	{
		HealthBar->SetFillColorAndOpacity(Color);
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

namespace MMOUI
{
	namespace Colors
	{
		const FLinearColor Panel(0.015f, 0.016f, 0.02f, 0.78f);
		const FLinearColor PanelOutline(0.55f, 0.45f, 0.28f, 0.55f);
		const FLinearColor BarBack(0.05f, 0.05f, 0.05f, 0.95f);
		const FLinearColor PlayerHealth(0.16f, 0.72f, 0.22f);
		const FLinearColor EnemyHealth(0.78f, 0.12f, 0.08f);
		const FLinearColor HealthChip(0.95f, 0.85f, 0.55f);
		const FLinearColor XP(0.55f, 0.28f, 0.88f);
		const FLinearColor Hostile(1.0f, 0.32f, 0.26f);
		const FLinearColor Gold(1.0f, 0.82f, 0.25f);
		const FLinearColor Dead(0.55f, 0.55f, 0.55f);
		const FLinearColor Error(1.0f, 0.28f, 0.22f);
		const FLinearColor TextDim(0.85f, 0.85f, 0.85f, 0.75f);
	}

	FSlateFontInfo Font(int32 Size, bool bBold)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		Info.OutlineSettings.OutlineSize = 1;
		Info.OutlineSettings.OutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);
		return Info;
	}

	FSlateBrush RoundedBrush(const FLinearColor& Color, float Radius, const FLinearColor& OutlineColor, float OutlineWidth)
	{
		return FSlateRoundedBoxBrush(Color, Radius, OutlineColor, OutlineWidth);
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold, ETextJustify::Type Justify)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(FText::FromString(Text));
		Block->SetFont(Font(Size, bBold));
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetJustification(Justify);
		return Block;
	}

	UProgressBar* MakeBar(UWidgetTree* Tree, const FLinearColor& FillColor, const FLinearColor& BackgroundColor, float Radius)
	{
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());

		FProgressBarStyle Style;
		Style.SetBackgroundImage(RoundedBrush(BackgroundColor, Radius));
		Style.SetFillImage(RoundedBrush(FLinearColor::White, Radius));
		Style.SetMarqueeImage(RoundedBrush(FLinearColor::White, Radius));
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(FillColor);
		Bar->SetPercent(1.0f);
		return Bar;
	}

	UButton* MakeButton(UWidgetTree* Tree, const FString& Label, int32 FontSize, UTextBlock** OutLabel)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());

		FButtonStyle Style;
		Style.SetNormal(RoundedBrush(FLinearColor(0.16f, 0.13f, 0.09f, 0.95f), 4.0f, Colors::PanelOutline, 1.0f));
		Style.SetHovered(RoundedBrush(FLinearColor(0.28f, 0.22f, 0.13f, 0.98f), 4.0f, Colors::Gold, 1.0f));
		Style.SetPressed(RoundedBrush(FLinearColor(0.10f, 0.08f, 0.05f, 1.0f), 4.0f, Colors::Gold, 1.0f));
		Style.SetDisabled(RoundedBrush(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), 4.0f));
		Style.SetNormalPadding(FMargin(10.0f, 3.0f));
		Style.SetPressedPadding(FMargin(10.0f, 4.0f, 10.0f, 2.0f));
		Button->SetStyle(Style);

		UTextBlock* Text = MakeText(Tree, Label, FontSize, FLinearColor::White, true, ETextJustify::Center);
		Button->SetContent(Text);
		if (OutLabel)
		{
			*OutLabel = Text;
		}
		return Button;
	}

	UWidget* MakeWindow(UWidgetTree* Tree, const FString& Title, float Width, UVerticalBox*& OutContent, UButton*& OutCloseButton, UTextBlock** OutTitle)
	{
		USizeBox* Root = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Root->SetWidthOverride(Width);

		UBorder* Frame = MakePanel(Tree, FLinearColor(0.02f, 0.022f, 0.028f, 0.94f), 8.0f, FMargin(12.0f, 8.0f, 12.0f, 12.0f), Colors::PanelOutline, 1.5f);
		Root->AddChild(Frame);

		UVerticalBox* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Frame->SetContent(Column);

		UHorizontalBox* TitleBar = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* TitleText = MakeText(Tree, Title, 16, Colors::Gold, true);
		UHorizontalBoxSlot* TitleSlot = TitleBar->AddChildToHorizontalBox(TitleText);
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
		OutCloseButton = MakeButton(Tree, TEXT("X"), 11);
		TitleBar->AddChildToHorizontalBox(OutCloseButton)->SetVerticalAlignment(VAlign_Center);
		Column->AddChildToVerticalBox(TitleBar)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));

		OutContent = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Column->AddChildToVerticalBox(OutContent);

		if (OutTitle)
		{
			*OutTitle = TitleText;
		}
		return Root;
	}

	UBorder* MakePanel(UWidgetTree* Tree, const FLinearColor& Color, float Radius, const FMargin& Padding, const FLinearColor& OutlineColor, float OutlineWidth)
	{
		UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Border->SetBrush(RoundedBrush(Color, Radius, OutlineColor, OutlineWidth));
		Border->SetPadding(Padding);
		return Border;
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMONPCPlateWidget.h"
#include "UI/MMOUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace MMONPCPlate
{
	static const FLinearColor Friendly(0.45f, 1.0f, 0.45f);
}

void UMMONPCPlateWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		WidgetTree->RootWidget = Root;
		MarkerText = MMOUI::MakeText(WidgetTree, TEXT(""), 34, MMOUI::Colors::Gold, true, ETextJustify::Center);
		NameText = MMOUI::MakeText(WidgetTree, TEXT(""), 13, MMONPCPlate::Friendly, true, ETextJustify::Center);
		TitleText = MMOUI::MakeText(WidgetTree, TEXT(""), 10, MMOUI::Colors::TextDim, false, ETextJustify::Center);
		for (UWidget* Child : { static_cast<UWidget*>(MarkerText), static_cast<UWidget*>(NameText), static_cast<UWidget*>(TitleText) })
		{
			Root->AddChildToVerticalBox(Child)->SetHorizontalAlignment(HAlign_Center);
		}
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMMONPCPlateWidget::SetPlate(const FText& Name, const FText& Title, EMMONPCMarker Marker)
{
	if (!bInitializedText && NameText && TitleText)
	{
		NameText->SetText(Name);
		TitleText->SetText(Title.IsEmpty() ? FText::GetEmpty() : FText::FromString(FString::Printf(TEXT("<%s>"), *Title.ToString())));
		bInitializedText = true;
	}

	if (Marker == CurrentMarker || !MarkerText)
	{
		return;
	}
	CurrentMarker = Marker;

	switch (Marker)
	{
	case EMMONPCMarker::QuestAvailable:
		MarkerText->SetText(FText::FromString(TEXT("!")));
		MarkerText->SetColorAndOpacity(FSlateColor(MMOUI::Colors::Gold));
		break;
	case EMMONPCMarker::QuestReady:
		MarkerText->SetText(FText::FromString(TEXT("?")));
		MarkerText->SetColorAndOpacity(FSlateColor(MMOUI::Colors::Gold));
		break;
	case EMMONPCMarker::QuestInProgress:
		MarkerText->SetText(FText::FromString(TEXT("?")));
		MarkerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)));
		break;
	default:
		MarkerText->SetText(FText::GetEmpty());
		break;
	}
}

// ---------------------------------------------------------------------------------------------------------------------

void UMMOTextButtonWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UTextBlock* LabelBlock = nullptr;
		Button = MMOUI::MakeButton(WidgetTree, TEXT(""), 13, &LabelBlock);
		Label = LabelBlock;
		WidgetTree->RootWidget = Button;
		Button->OnClicked.AddDynamic(this, &UMMOTextButtonWidget::HandleClicked);
	}
}

void UMMOTextButtonWidget::Setup(const FString& InLabel, const FLinearColor& Color, int32 FontSize, bool bLeftAligned)
{
	if (Label)
	{
		Label->SetText(FText::FromString(InLabel));
		Label->SetColorAndOpacity(FSlateColor(Color));
		Label->SetFont(MMOUI::Font(FontSize, true));
		Label->SetJustification(bLeftAligned ? ETextJustify::Left : ETextJustify::Center);
	}
}

void UMMOTextButtonWidget::HandleClicked()
{
	OnClickedNative.ExecuteIfBound();
}

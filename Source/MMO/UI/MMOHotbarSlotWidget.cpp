// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHotbarSlotWidget.h"
#include "UI/MMOUIStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace MMOHotbar
{
	static const FLinearColor SlotIdle(0.20f, 0.16f, 0.12f, 0.95f);
	static const FLinearColor SlotOutOfRange(0.42f, 0.07f, 0.05f, 0.95f);
}

void UMMOHotbarSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildDefaultLayout();
	}
}

void UMMOHotbarSlotWidget::BuildDefaultLayout()
{
	using namespace MMOUI;

	UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	WidgetTree->RootWidget = Root;

	USizeBox* SlotBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SlotBox->SetWidthOverride(68.0f);
	SlotBox->SetHeightOverride(68.0f);
	Root->AddChildToVerticalBox(SlotBox)->SetHorizontalAlignment(HAlign_Center);

	UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SlotBox->AddChild(Layers);

	auto AddFill = [Layers](UWidget* Child)
	{
		UOverlaySlot* LayerSlot = Layers->AddChildToOverlay(Child);
		LayerSlot->SetHorizontalAlignment(HAlign_Fill);
		LayerSlot->SetVerticalAlignment(VAlign_Fill);
		return LayerSlot;
	};

	SlotBackground = MakePanel(WidgetTree, MMOHotbar::SlotIdle, 6.0f, FMargin(0.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.9f), 2.0f);
	AddFill(SlotBackground);

	LabelText = MakeText(WidgetTree, TEXT("Attack"), 12, FLinearColor::White, true, ETextJustify::Center);
	UOverlaySlot* LabelSlot = Layers->AddChildToOverlay(LabelText);
	LabelSlot->SetHorizontalAlignment(HAlign_Center);
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	SwingTimerBar = MakeBar(WidgetTree, FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), FLinearColor::Transparent, 6.0f);
	SwingTimerBar->SetBarFillType(EProgressBarFillType::TopToBottom);
	SwingTimerBar->SetPercent(0.0f);
	AddFill(SwingTimerBar)->SetPadding(FMargin(2.0f));

	ActiveFrame = MakePanel(WidgetTree, FLinearColor::Transparent, 6.0f, FMargin(0.0f), Colors::Gold, 3.0f);
	ActiveFrame->SetVisibility(ESlateVisibility::Hidden);
	AddFill(ActiveFrame);

	KeyText = MakeText(WidgetTree, TEXT("1"), 12, Colors::Gold, true);
	UOverlaySlot* KeySlot = Layers->AddChildToOverlay(KeyText);
	KeySlot->SetHorizontalAlignment(HAlign_Left);
	KeySlot->SetVerticalAlignment(VAlign_Top);
	KeySlot->SetPadding(FMargin(6.0f, 3.0f, 0.0f, 0.0f));

	StateText = MakeText(WidgetTree, TEXT("Auto Attack"), 10, Colors::TextDim, true, ETextJustify::Center);
	Root->AddChildToVerticalBox(StateText)->SetHorizontalAlignment(HAlign_Center);
}

void UMMOHotbarSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bActive && ActiveFrame)
	{
		PulseTime += InDeltaTime;
		ActiveFrame->SetRenderOpacity(0.65f + 0.35f * FMath::Sin(PulseTime * 6.0f));
	}
}

void UMMOHotbarSlotWidget::SetSlotState(bool bAutoAttackActive, bool bOutOfRange, float SwingRemaining, float SwingInterval)
{
	if (bActive != bAutoAttackActive)
	{
		bActive = bAutoAttackActive;
		PulseTime = 0.0f;
		if (ActiveFrame)
		{
			ActiveFrame->SetVisibility(bActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		}
		if (StateText)
		{
			StateText->SetText(bActive ? NSLOCTEXT("MMOHUD", "AutoOn", "Auto Attack: ON") : NSLOCTEXT("MMOHUD", "AutoOff", "Auto Attack"));
			StateText->SetColorAndOpacity(FSlateColor(bActive ? MMOUI::Colors::Gold : MMOUI::Colors::TextDim));
		}
	}

	if (SlotBackground)
	{
		SlotBackground->SetBrushColor(bOutOfRange ? MMOHotbar::SlotOutOfRange : MMOHotbar::SlotIdle);
	}

	if (SwingTimerBar)
	{
		SwingTimerBar->SetPercent(SwingInterval > 0.0f ? FMath::Clamp(SwingRemaining / SwingInterval, 0.0f, 1.0f) : 0.0f);
	}
}

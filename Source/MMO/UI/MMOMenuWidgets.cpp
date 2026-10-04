// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOMenuWidgets.h"
#include "UI/MMONPCPlateWidget.h"
#include "UI/MMOUIStyle.h"
#include "Settings/MMOSettingsSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace MMOMenuUI
{
	static UMMOTextButtonWidget* AddMenuButton(UWidgetTree* Tree, UVerticalBox* Box, const FString& Label, const FLinearColor& Color, TFunction<void()> OnClick, float Width = 280.0f)
	{
		USizeBox* Size = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(Width);
		Size->SetHeightOverride(40.0f);
		UMMOTextButtonWidget* Button = Tree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
		Button->Setup(Label, Color, 16);
		Button->OnClickedNative.BindLambda(MoveTemp(OnClick));
		Size->AddChild(Button);
		UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Size);
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
		ButtonSlot->SetPadding(FMargin(0.0f, 5.0f));
		return Button;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Title

void UMMOTitleScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Shade = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Shade->SetBrushColor(FLinearColor(0.01f, 0.01f, 0.015f, 0.72f));
		Shade->SetHorizontalAlignment(HAlign_Center);
		Shade->SetVerticalAlignment(VAlign_Center);
		WidgetTree->RootWidget = Shade;

		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Shade->SetContent(Column);

		UTextBlock* Title = MMOUI::MakeText(WidgetTree, TEXT("THORNWICK"), 64, MMOUI::Colors::Gold, true, ETextJustify::Center);
		Column->AddChildToVerticalBox(Title)->SetHorizontalAlignment(HAlign_Center);
		UTextBlock* Subtitle = MMOUI::MakeText(WidgetTree, TEXT("A classic MMO, reinvented"), 20, FLinearColor(0.85f, 0.82f, 0.75f), false, ETextJustify::Center);
		Column->AddChildToVerticalBox(Subtitle)->SetHorizontalAlignment(HAlign_Center);

		Buttons = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Column->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.0f, 40.0f, 0.0f, 0.0f));

		FooterText = MMOUI::MakeText(WidgetTree, TEXT(""), 13, MMOUI::Colors::TextDim, false, ETextJustify::Center);
		Column->AddChildToVerticalBox(FooterText)->SetPadding(FMargin(0.0f, 28.0f, 0.0f, 0.0f));
	}
}

void UMMOTitleScreenWidget::Setup(bool bHasSave, int32 SavedLevel)
{
	bHasSavedCharacter = bHasSave;
	Level = SavedLevel;
	bConfirmNewAdventure = false;
	Rebuild();
}

void UMMOTitleScreenWidget::Rebuild()
{
	if (!Buttons)
	{
		return;
	}
	Buttons->ClearChildren();
	const FLinearColor Light(0.95f, 0.92f, 0.85f);

	if (bHasSavedCharacter)
	{
		MMOMenuUI::AddMenuButton(WidgetTree, Buttons, FString::Printf(TEXT("Continue  (level %d)"), Level), MMOUI::Colors::Gold, [this]() { OnContinue.ExecuteIfBound(); });
		const FString NewLabel = bConfirmNewAdventure ? TEXT("Click again: erase your character?") : TEXT("New Adventure");
		MMOMenuUI::AddMenuButton(WidgetTree, Buttons, NewLabel, bConfirmNewAdventure ? MMOUI::Colors::Error : Light, [this]()
		{
			if (bConfirmNewAdventure)
			{
				OnNewAdventure.ExecuteIfBound();
			}
			else
			{
				bConfirmNewAdventure = true;
				Rebuild();
			}
		});
	}
	else
	{
		MMOMenuUI::AddMenuButton(WidgetTree, Buttons, TEXT("Begin Your Adventure"), MMOUI::Colors::Gold, [this]() { OnContinue.ExecuteIfBound(); });
	}
	MMOMenuUI::AddMenuButton(WidgetTree, Buttons, TEXT("Settings"), Light, [this]() { OnSettings.ExecuteIfBound(); });
	MMOMenuUI::AddMenuButton(WidgetTree, Buttons, TEXT("Quit"), Light, [this]() { OnQuit.ExecuteIfBound(); });

	FooterText->SetText(FText::FromString(bHasSavedCharacter ? TEXT("Your progress is saved automatically.") : TEXT("A wolf howls somewhere beyond the village gate...")));
}

// ---------------------------------------------------------------------------------------------------------------------
// Game menu

void UMMOGameMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT("Game Menu"), 320.0f, Content, CloseWidget);
		CloseWidget->SetVisibility(ESlateVisibility::Collapsed);

		const FLinearColor Light(0.95f, 0.92f, 0.85f);
		MMOMenuUI::AddMenuButton(WidgetTree, Content, TEXT("Resume"), MMOUI::Colors::Gold, [this]() { OnResume.ExecuteIfBound(); }, 260.0f);
		MMOMenuUI::AddMenuButton(WidgetTree, Content, TEXT("Settings"), Light, [this]() { OnSettings.ExecuteIfBound(); }, 260.0f);
		MMOMenuUI::AddMenuButton(WidgetTree, Content, TEXT("Controls"), Light, [this]() { OnControls.ExecuteIfBound(); }, 260.0f);
		MMOMenuUI::AddMenuButton(WidgetTree, Content, TEXT("Save & Quit to Title"), Light, [this]() { OnQuitToTitle.ExecuteIfBound(); }, 260.0f);
		MMOMenuUI::AddMenuButton(WidgetTree, Content, TEXT("Save & Quit Game"), Light, [this]() { OnQuitGame.ExecuteIfBound(); }, 260.0f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Settings

void UMMOSettingsWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT("Settings"), 460.0f, Content, CloseWidget);
		CloseButton = CloseWidget;
		CloseButton->OnClicked.AddDynamic(this, &UMMOSettingsWindowWidget::HandleClose);
		Rows = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(Rows);
		UTextBlock* Hint = MMOUI::MakeText(WidgetTree, TEXT("Changes apply and save immediately."), 11, MMOUI::Colors::TextDim);
		Content->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
}

void UMMOSettingsWindowWidget::Init(UMMOSettingsSubsystem* InSettings)
{
	Settings = InSettings;
	Refresh();
}

void UMMOSettingsWindowWidget::AddRow(const FString& Label, const FString& Value, TFunction<void(int32)> OnStep)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(MMOUI::MakeText(WidgetTree, Label, 14, FLinearColor(0.9f, 0.88f, 0.82f)));
	LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	LabelSlot->SetVerticalAlignment(VAlign_Center);

	TSharedRef<TFunction<void(int32)>> Step = MakeShared<TFunction<void(int32)>>(MoveTemp(OnStep));
	auto MakeArrow = [this, Row, Step](const TCHAR* Text, int32 Direction)
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Size->SetWidthOverride(34.0f);
		UMMOTextButtonWidget* Button = WidgetTree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
		Button->Setup(Text, MMOUI::Colors::Gold, 14);
		Button->OnClickedNative.BindLambda([this, Step, Direction]() { (*Step)(Direction); Refresh(); });
		Size->AddChild(Button);
		Row->AddChildToHorizontalBox(Size)->SetVerticalAlignment(VAlign_Center);
	};

	MakeArrow(TEXT("<"), -1);
	USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ValueBox->SetWidthOverride(130.0f);
	ValueBox->AddChild(MMOUI::MakeText(WidgetTree, Value, 14, FLinearColor::White, true, ETextJustify::Center));
	Row->AddChildToHorizontalBox(ValueBox)->SetVerticalAlignment(VAlign_Center);
	MakeArrow(TEXT(">"), 1);

	Rows->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 3.0f));
}

void UMMOSettingsWindowWidget::Refresh()
{
	UMMOSettingsSubsystem* Subsystem = Settings.Get();
	if (!Rows || !Subsystem)
	{
		return;
	}
	UMMOSettingsSave* S = Subsystem->Get();
	Rows->ClearChildren();

	TWeakObjectPtr<UMMOSettingsSubsystem> Weak = Subsystem;
	auto Change = [Weak](TFunction<void(UMMOSettingsSave*)> Edit)
	{
		if (Weak.IsValid())
		{
			Edit(Weak->Get());
			Weak->ApplyAndSave();
		}
	};
	auto OnOff = [](bool b) { return FString(b ? TEXT("On") : TEXT("Off")); };

	AddRow(TEXT("Master volume"), FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S->MasterVolume * 100.0f)),
		[Change](int32 D) { Change([D](UMMOSettingsSave* X) { X->MasterVolume = FMath::Clamp(FMath::RoundToFloat((X->MasterVolume + D * 0.1f) * 10.0f) / 10.0f, 0.0f, 1.0f); }); });
	AddRow(TEXT("Mouse sensitivity"), FString::Printf(TEXT("%.1f"), S->MouseSensitivity),
		[Change](int32 D) { Change([D](UMMOSettingsSave* X) { X->MouseSensitivity = FMath::Clamp(FMath::RoundToFloat((X->MouseSensitivity + D * 0.1f) * 10.0f) / 10.0f, 0.2f, 3.0f); }); });
	AddRow(TEXT("Invert camera Y"), OnOff(S->bInvertY), [Change](int32) { Change([](UMMOSettingsSave* X) { X->bInvertY = !X->bInvertY; }); });
	AddRow(TEXT("Graphics quality"), UMMOSettingsSubsystem::GetQualityName(S->GraphicsQuality).ToString(),
		[Change](int32 D) { Change([D](UMMOSettingsSave* X) { X->GraphicsQuality = (X->GraphicsQuality + D + 4) % 4; }); });
	AddRow(TEXT("Window"), S->bFullscreen ? TEXT("Fullscreen") : TEXT("Windowed"), [Change](int32) { Change([](UMMOSettingsSave* X) { X->bFullscreen = !X->bFullscreen; }); });
	AddRow(TEXT("VSync"), OnOff(S->bVSync), [Change](int32) { Change([](UMMOSettingsSave* X) { X->bVSync = !X->bVSync; }); });
	AddRow(TEXT("Frame rate limit"), S->FrameRateLimit > 0 ? FString::Printf(TEXT("%d"), S->FrameRateLimit) : TEXT("Unlimited"),
		[Change](int32 D)
		{
			Change([D](UMMOSettingsSave* X)
			{
				static const int32 Limits[] = { 0, 30, 60, 120, 144 };
				int32 Index = 0;
				for (int32 i = 0; i < UE_ARRAY_COUNT(Limits); ++i) { if (Limits[i] == X->FrameRateLimit) { Index = i; } }
				X->FrameRateLimit = Limits[(Index + D + UE_ARRAY_COUNT(Limits)) % UE_ARRAY_COUNT(Limits)];
			});
		});
	AddRow(TEXT("Key hints"), OnOff(S->bShowKeyHints), [Change](int32) { Change([](UMMOSettingsSave* X) { X->bShowKeyHints = !X->bShowKeyHints; }); });
	AddRow(TEXT("Tutorial tips"), OnOff(S->bShowTutorial), [Change](int32) { Change([](UMMOSettingsSave* X) { X->bShowTutorial = !X->bShowTutorial; }); });
}

void UMMOSettingsWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

// ---------------------------------------------------------------------------------------------------------------------
// Controls

void UMMOControlsWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT("Controls"), 470.0f, Content, CloseWidget);
		CloseButton = CloseWidget;
		CloseButton->OnClicked.AddDynamic(this, &UMMOControlsWindowWidget::HandleClose);

		static const TCHAR* Lines[][2] = {
			{ TEXT("W A S D"), TEXT("Move") },
			{ TEXT("Space"), TEXT("Jump") },
			{ TEXT("Hold mouse button + drag"), TEXT("Turn the camera") },
			{ TEXT("Mouse wheel"), TEXT("Zoom") },
			{ TEXT("Left-click"), TEXT("Select a target") },
			{ TEXT("Right-click"), TEXT("Attack, loot, talk, gather, use") },
			{ TEXT("F"), TEXT("Talk / loot / use what's nearby") },
			{ TEXT("Tab"), TEXT("Next target") },
			{ TEXT("1"), TEXT("Auto attack on / off") },
			{ TEXT("2 - 9"), TEXT("Hotbar: abilities, potions, food") },
			{ TEXT("B or I"), TEXT("Backpack") },
			{ TEXT("C"), TEXT("Character and professions") },
			{ TEXT("K"), TEXT("Abilities") },
			{ TEXT("L"), TEXT("Quest log") },
			{ TEXT("Esc"), TEXT("Close windows, clear target, game menu") },
		};
		for (const auto& Line : Lines)
		{
			UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			KeyBox->SetWidthOverride(200.0f);
			KeyBox->AddChild(MMOUI::MakeText(WidgetTree, Line[0], 13, MMOUI::Colors::Gold, true));
			Row->AddChildToHorizontalBox(KeyBox);
			Row->AddChildToHorizontalBox(MMOUI::MakeText(WidgetTree, Line[1], 13, FLinearColor(0.9f, 0.88f, 0.82f)));
			Content->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 2.0f));
		}
	}
}

void UMMOControlsWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

// ---------------------------------------------------------------------------------------------------------------------
// Guide

void UMMOGuideWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Panel = MMOUI::MakePanel(WidgetTree, FLinearColor(0.03f, 0.03f, 0.04f, 0.82f), 6.0f, FMargin(10.0f, 8.0f), FLinearColor(0.75f, 0.6f, 0.25f, 0.9f), 1.5f);
		WidgetTree->RootWidget = Panel;
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Width->SetWidthOverride(330.0f);
		Panel->SetContent(Width);
		UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Width->AddChild(Column);

		Column->AddChildToVerticalBox(MMOUI::MakeText(WidgetTree, TEXT("Guide"), 13, MMOUI::Colors::Gold, true));
		TipText = MMOUI::MakeText(WidgetTree, TEXT(""), 13, FLinearColor(0.92f, 0.9f, 0.84f));
		TipText->SetWrapTextAt(310.0f);
		Column->AddChildToVerticalBox(TipText)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 4.0f));

		UMMOTextButtonWidget* Hide = WidgetTree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
		Hide->Setup(TEXT("Hide tips"), MMOUI::Colors::TextDim, 11);
		Hide->OnClickedNative.BindLambda([this]() { OnHideTips.ExecuteIfBound(); });
		Column->AddChildToVerticalBox(Hide)->SetHorizontalAlignment(HAlign_Right);
	}
}

void UMMOGuideWidget::SetTip(const FText& Tip)
{
	if (TipText)
	{
		TipText->SetText(Tip);
	}
}

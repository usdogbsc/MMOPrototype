// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOAbilityWidgets.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "Combat/MMOAbilityComponent.h"
#include "Combat/MMOAbilityDefinition.h"
#include "Combat/MMOProgressionComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"

namespace MMOAbilityUI
{
	FString Initials(const FText& Name)
	{
		FString Result;
		TArray<FString> Words;
		Name.ToString().ParseIntoArrayWS(Words);
		for (const FString& Word : Words)
		{
			if (!Word.IsEmpty() && Result.Len() < 2)
			{
				Result.AppendChar(FChar::ToUpper(Word[0]));
			}
		}
		return Result;
	}

	static FString FormatSeconds(float Seconds)
	{
		return Seconds >= 60.0f ? FString::Printf(TEXT("%d min"), FMath::RoundToInt(Seconds / 60.0f)) : FString::Printf(TEXT("%s sec"), *FString::SanitizeFloat(Seconds, 0));
	}

	static const FLinearColor AbilityColor(1.0f, 0.82f, 0.4f);
}

// ---------------------------------------------------------------------------------------------------------------------
// Tooltip

void UMMOAbilityTooltipWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UBorder* Panel = MMOUI::MakePanel(WidgetTree, FLinearColor(0.03f, 0.03f, 0.04f, 0.96f), 6.0f, FMargin(10.0f, 8.0f), FLinearColor(0.45f, 0.4f, 0.3f, 1.0f), 1.5f);
		USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Width->SetMaxDesiredWidth(300.0f);
		Lines = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Width->AddChild(Lines);
		Panel->SetContent(Width);
		WidgetTree->RootWidget = Panel;
	}
}

void UMMOAbilityTooltipWidget::SetAbility(const UMMOAbilityDefinition* Ability, bool bKnown)
{
	if (!Lines || !Ability)
	{
		return;
	}
	Lines->ClearChildren();

	auto AddLine = [this](const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false, float Top = 0.0f)
	{
		if (Text.IsEmpty())
		{
			return;
		}
		UTextBlock* Line = MMOUI::MakeText(WidgetTree, Text, Size, Color, bBold);
		Line->SetWrapTextAt(280.0f);
		Lines->AddChildToVerticalBox(Line)->SetPadding(FMargin(0.0f, Top, 0.0f, 0.0f));
	};

	AddLine(Ability->DisplayName.ToString(), 15, MMOAbilityUI::AbilityColor, true);
	const FString Cast = Ability->CastTime > 0.0f ? FString::Printf(TEXT("%s cast"), *MMOAbilityUI::FormatSeconds(Ability->CastTime)) : TEXT("Instant");
	const FString Range = Ability->TargetType == EMMOAbilityTarget::Self ? TEXT("Self") : (Ability->TargetType == EMMOAbilityTarget::EnemiesInFront ? TEXT("Enemies in front") : TEXT("Melee range"));
	AddLine(FString::Printf(TEXT("%s    %s"), *Cast, *Range), 12, FLinearColor::White, false, 4.0f);
	if (Ability->Cooldown > 0.0f)
	{
		AddLine(FString::Printf(TEXT("Cooldown: %s"), *MMOAbilityUI::FormatSeconds(Ability->Cooldown)), 12, FLinearColor::White);
	}
	AddLine(Ability->Description.ToString(), 12, FLinearColor(0.88f, 0.86f, 0.8f), false, 6.0f);
	if (!bKnown)
	{
		AddLine(FString::Printf(TEXT("Learned at level %d"), Ability->RequiredLevel), 12, MMOUI::Colors::Error, false, 6.0f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Icon

void UMMOAbilityIconWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		WidgetTree->RootWidget = Root;
		UOverlay* Layers = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		Root->AddChild(Layers);

		Frame = MMOUI::MakePanel(WidgetTree, FLinearColor(0.06f, 0.06f, 0.07f, 0.95f), 5.0f, FMargin(0.0f), MMOAbilityUI::AbilityColor * 0.7f, 2.0f);
		UOverlaySlot* FrameSlot = Layers->AddChildToOverlay(Frame);
		FrameSlot->SetHorizontalAlignment(HAlign_Fill);
		FrameSlot->SetVerticalAlignment(VAlign_Fill);

		Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		UOverlaySlot* IconSlot = Layers->AddChildToOverlay(Icon);
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
		IconSlot->SetPadding(FMargin(4.0f));

		Initials = MMOUI::MakeText(WidgetTree, TEXT(""), 14, MMOAbilityUI::AbilityColor, true, ETextJustify::Center);
		UOverlaySlot* TextSlot = Layers->AddChildToOverlay(Initials);
		TextSlot->SetHorizontalAlignment(HAlign_Center);
		TextSlot->SetVerticalAlignment(VAlign_Center);
	}
}

void UMMOAbilityIconWidget::Setup(AMMOCharacter* InCharacter, UMMOAbilityDefinition* InAbility, float InSize)
{
	Character = InCharacter;
	Ability = InAbility;
	Size = InSize;
	if (USizeBox* Root = Cast<USizeBox>(WidgetTree->RootWidget))
	{
		Root->SetWidthOverride(Size);
		Root->SetHeightOverride(Size);
	}
	if (!InAbility)
	{
		return;
	}

	UTexture2D* Texture = InAbility->Icon.Get();
	if (Texture)
	{
		Icon->SetBrushFromTexture(Texture);
	}
	Icon->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	Initials->SetText(FText::FromString(Texture ? FString() : MMOAbilityUI::Initials(InAbility->DisplayName)));

	const bool bKnown = IsKnown();
	SetRenderOpacity(bKnown ? 1.0f : 0.4f);

	if (!Tooltip)
	{
		Tooltip = CreateWidget<UMMOAbilityTooltipWidget>(this, UMMOAbilityTooltipWidget::StaticClass());
	}
	Tooltip->SetAbility(InAbility, bKnown);
	SetToolTip(Tooltip);
}

bool UMMOAbilityIconWidget::IsKnown() const
{
	return Character.IsValid() && Ability.IsValid() && Character->GetAbilities()->IsKnown(Ability.Get());
}

FReply UMMOAbilityIconWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && IsKnown())
	{
		return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
	}
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && IsKnown())
	{
		Character->GetAbilities()->UseAbility(Ability.Get());
	}
	return FReply::Handled();
}

void UMMOAbilityIconWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!IsKnown())
	{
		return;
	}
	UMMOAbilityDragOperation* Operation = NewObject<UMMOAbilityDragOperation>(this);
	Operation->AbilityId = Ability->AbilityId;
	Operation->Pivot = EDragPivot::CenterCenter;

	UMMOAbilityIconWidget* Visual = CreateWidget<UMMOAbilityIconWidget>(this, UMMOAbilityIconWidget::StaticClass());
	Visual->Setup(Character.Get(), Ability.Get(), Size);
	Visual->SetRenderOpacity(0.85f);
	Operation->DefaultDragVisual = Visual;
	OutOperation = Operation;
}

// ---------------------------------------------------------------------------------------------------------------------
// Window

void UMMOAbilitiesWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT("Abilities"), 420.0f, Content, CloseWidget);
		CloseButton = CloseWidget;
		CloseButton->OnClicked.AddDynamic(this, &UMMOAbilitiesWindowWidget::HandleClose);

		List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(List);
		UTextBlock* Hint = MMOUI::MakeText(WidgetTree, TEXT("Drag an ability onto the hotbar. Right-click to use it."), 11, MMOUI::Colors::TextDim);
		Content->AddChildToVerticalBox(Hint)->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 0.0f));
	}
}

void UMMOAbilitiesWindowWidget::Init(AMMOCharacter* InCharacter)
{
	Character = InCharacter;
	Refresh();
}

void UMMOAbilitiesWindowWidget::Refresh()
{
	AMMOCharacter* Owner = Character.Get();
	if (!List || !Owner)
	{
		return;
	}
	List->ClearChildren();

	const int32 Level = Owner->GetProgression()->GetLevel();
	for (UMMOAbilityDefinition* Ability : Owner->GetAbilities()->GetAllAbilities())
	{
		const bool bKnown = Owner->GetAbilities()->IsKnown(Ability);
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UMMOAbilityIconWidget* IconWidget = WidgetTree->ConstructWidget<UMMOAbilityIconWidget>(UMMOAbilityIconWidget::StaticClass());
		IconWidget->Setup(Owner, Ability, 44.0f);
		UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(IconWidget);
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Top);
		IconSlot->SetHorizontalAlignment(HAlign_Left);

		UVerticalBox* Text = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Text->AddChildToVerticalBox(MMOUI::MakeText(WidgetTree, Ability->DisplayName.ToString(), 14, bKnown ? MMOAbilityUI::AbilityColor : FLinearColor(0.55f, 0.55f, 0.55f), true));
		const FString Status = bKnown ? FString::Printf(TEXT("Learned at level %d"), Ability->RequiredLevel)
			: FString::Printf(TEXT("Learn at level %d (%d more)"), Ability->RequiredLevel, Ability->RequiredLevel - Level);
		Text->AddChildToVerticalBox(MMOUI::MakeText(WidgetTree, Status, 11, bKnown ? MMOUI::Colors::TextDim : MMOUI::Colors::Error));
		UTextBlock* Description = MMOUI::MakeText(WidgetTree, Ability->Description.ToString(), 11, FLinearColor(0.85f, 0.83f, 0.78f));
		Description->SetWrapTextAt(320.0f);
		Text->AddChildToVerticalBox(Description);
		UHorizontalBoxSlot* TextSlot = Row->AddChildToHorizontalBox(Text);
		TextSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TextSlot->SetVerticalAlignment(VAlign_Center);

		List->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f));
	}
}

void UMMOAbilitiesWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

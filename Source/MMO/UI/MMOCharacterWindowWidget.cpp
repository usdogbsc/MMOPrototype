// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOCharacterWindowWidget.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "MMOCharacterWindow"

namespace MMOCharacterWindow
{
	static const EMMOEquipmentSlot LeftColumn[] = { EMMOEquipmentSlot::Head, EMMOEquipmentSlot::Chest, EMMOEquipmentSlot::Hands, EMMOEquipmentSlot::Legs, EMMOEquipmentSlot::Feet };
	static const EMMOEquipmentSlot RightColumn[] = { EMMOEquipmentSlot::MainHand, EMMOEquipmentSlot::OffHand, EMMOEquipmentSlot::Amulet, EMMOEquipmentSlot::Ring };
}

void UMMOCharacterWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
}

void UMMOCharacterWindowWidget::BuildLayout()
{
	using namespace MMOUI;

	UVerticalBox* Content = nullptr;
	UButton* CloseButtonWidget = nullptr;
	UTextBlock* TitleWidget = nullptr;
	WidgetTree->RootWidget = MakeWindow(WidgetTree, TEXT("Character"), 380.0f, Content, CloseButtonWidget, &TitleWidget);
	CloseButton = CloseButtonWidget;
	CloseButton->OnClicked.AddDynamic(this, &UMMOCharacterWindowWidget::HandleClose);

	HeaderText = MakeText(WidgetTree, TEXT(""), 14, FLinearColor::White, true, ETextJustify::Center);
	Content->AddChildToVerticalBox(HeaderText)->SetHorizontalAlignment(HAlign_Center);

	// equipment: two columns of labelled slots
	UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* Left = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBox* Right = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	for (EMMOEquipmentSlot EquipSlot : MMOCharacterWindow::LeftColumn)
	{
		Left->AddChildToVerticalBox(MakeSlotRow(EquipSlot, false))->SetPadding(FMargin(0.0f, 2.0f));
	}
	for (EMMOEquipmentSlot EquipSlot : MMOCharacterWindow::RightColumn)
	{
		Right->AddChildToVerticalBox(MakeSlotRow(EquipSlot, true))->SetPadding(FMargin(0.0f, 2.0f));
	}
	UHorizontalBoxSlot* LeftSlot = Columns->AddChildToHorizontalBox(Left);
	LeftSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UHorizontalBoxSlot* RightSlot = Columns->AddChildToHorizontalBox(Right);
	RightSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	RightSlot->SetHorizontalAlignment(HAlign_Right);
	Content->AddChildToVerticalBox(Columns)->SetPadding(FMargin(0.0f, 8.0f));

	// stats
	UBorder* StatsPanel = MakePanel(WidgetTree, FLinearColor(0.05f, 0.05f, 0.06f, 0.9f), 5.0f, FMargin(10.0f, 6.0f));
	UVerticalBox* Stats = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	StatsPanel->SetContent(Stats);
	HealthStatText = AddStatRow(Stats, TEXT("Health"));
	DamageStatText = AddStatRow(Stats, TEXT("Damage per swing"));
	ArmorStatText = AddStatRow(Stats, TEXT("Armor"));
	SwingStatText = AddStatRow(Stats, TEXT("Swing interval"));
	Content->AddChildToVerticalBox(StatsPanel);
}

UWidget* UMMOCharacterWindowWidget::MakeSlotRow(EMMOEquipmentSlot EquipSlot, bool bLabelOnLeft)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	UMMOItemSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UMMOItemSlotWidget>(UMMOItemSlotWidget::StaticClass());
	SlotWidget->Configure(EMMOItemSlotContext::Equipment, INDEX_NONE, EquipSlot, FText::GetEmpty(), 46.0f);
	SlotWidget->OnRightClicked.BindUObject(this, &UMMOCharacterWindowWidget::HandleSlotUse);
	SlotWidget->OnDoubleClicked.BindUObject(this, &UMMOCharacterWindowWidget::HandleSlotUse);
	SlotWidget->OnDropped.BindUObject(this, &UMMOCharacterWindowWidget::HandleSlotDropped);
	SlotWidgets.Add(EquipSlot, SlotWidget);

	UTextBlock* Label = MMOUI::MakeText(WidgetTree, MMOItems::GetSlotText(EquipSlot).ToString(), 11, MMOUI::Colors::TextDim, false, bLabelOnLeft ? ETextJustify::Right : ETextJustify::Left);

	if (bLabelOnLeft)
	{
		Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
		Row->AddChildToHorizontalBox(SlotWidget)->SetPadding(FMargin(6.0f, 0.0f, 0.0f, 0.0f));
	}
	else
	{
		Row->AddChildToHorizontalBox(SlotWidget)->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		Row->AddChildToHorizontalBox(Label)->SetVerticalAlignment(VAlign_Center);
	}
	return Row;
}

UTextBlock* UMMOCharacterWindowWidget::AddStatRow(UVerticalBox* Box, const FString& Label)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Name = MMOUI::MakeText(WidgetTree, Label, 12, MMOUI::Colors::TextDim);
	UTextBlock* Value = MMOUI::MakeText(WidgetTree, TEXT(""), 12, FLinearColor::White, true, ETextJustify::Right);
	Row->AddChildToHorizontalBox(Name)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	Row->AddChildToHorizontalBox(Value);
	Box->AddChildToVerticalBox(Row);
	return Value;
}

void UMMOCharacterWindowWidget::Init(AMMOCharacter* InCharacter)
{
	if (Character.Get() == InCharacter)
	{
		return;
	}

	if (AMMOCharacter* Old = Character.Get())
	{
		Old->GetEquipment()->OnEquipmentChanged.RemoveAll(this);
		Old->GetInventory()->OnInventoryChanged.RemoveAll(this);
	}

	Character = InCharacter;
	if (InCharacter)
	{
		InCharacter->GetEquipment()->OnEquipmentChanged.AddDynamic(this, &UMMOCharacterWindowWidget::Refresh);
		InCharacter->GetInventory()->OnInventoryChanged.AddDynamic(this, &UMMOCharacterWindowWidget::Refresh);
	}
	Refresh();
}

UMMOItemSlotWidget* UMMOCharacterWindowWidget::GetSlotWidget(EMMOEquipmentSlot EquipSlot) const
{
	const TObjectPtr<UMMOItemSlotWidget>* Found = SlotWidgets.Find(EquipSlot);
	return Found ? Found->Get() : nullptr;
}

void UMMOCharacterWindowWidget::Refresh()
{
	const AMMOCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return;
	}

	for (const TPair<EMMOEquipmentSlot, TObjectPtr<UMMOItemSlotWidget>>& Pair : SlotWidgets)
	{
		Pair.Value->SetStack(Owner->GetEquipment()->GetEquipped(Pair.Key));
	}
	RefreshStats();
}

void UMMOCharacterWindowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// health and level change outside equipment events
	RefreshStats();
}

void UMMOCharacterWindowWidget::RefreshStats()
{
	const AMMOCharacter* Owner = Character.Get();
	if (!Owner)
	{
		return;
	}

	const UMMOHealthComponent* Health = Owner->GetHealth();
	const UMMOCombatComponent* Combat = Owner->GetCombat();

	if (HeaderText)
	{
		HeaderText->SetText(FText::Format(LOCTEXT("Header", "Adventurer  -  Level {0}"), Owner->GetProgression()->GetLevel()));
	}
	if (HealthStatText)
	{
		HealthStatText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), FMath::CeilToInt(Health->GetCurrentHealth()), FMath::RoundToInt(Health->GetMaxHealth()))));
	}
	if (DamageStatText)
	{
		float Min, Max;
		Combat->GetDamageRange(Min, Max);
		DamageStatText->SetText(FText::FromString(FString::Printf(TEXT("%d - %d"), FMath::RoundToInt(Min), FMath::RoundToInt(Max))));
	}
	if (ArmorStatText)
	{
		ArmorStatText->SetText(FText::FromString(FString::Printf(TEXT("%d  (-%d%% damage taken)"), FMath::RoundToInt(Health->GetArmor()), FMath::RoundToInt(Health->GetDamageReduction() * 100.0f))));
	}
	if (SwingStatText)
	{
		SwingStatText->SetText(FText::FromString(FString::Printf(TEXT("%.1fs"), Combat->BasicAttackCooldown)));
	}
}

void UMMOCharacterWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

void UMMOCharacterWindowWidget::HandleSlotUse(UMMOItemSlotWidget* SlotWidget)
{
	if (AMMOCharacter* Owner = Character.Get())
	{
		if (SlotWidget && !SlotWidget->GetStack().IsEmpty())
		{
			Owner->UnequipSlot(SlotWidget->GetEquipSlot());
		}
	}
}

void UMMOCharacterWindowWidget::HandleSlotDropped(UMMOItemSlotWidget* Target, UMMOItemSlotWidget* Source)
{
	AMMOCharacter* Owner = Character.Get();
	if (!Owner || !Target || !Source || Source->GetContext() != EMMOItemSlotContext::Inventory || Source->GetStack().IsEmpty())
	{
		return;
	}

	if (Source->GetStack().Item->EquipmentSlot == Target->GetEquipSlot())
	{
		Owner->EquipInventorySlot(Source->GetSlotIndex());
	}
	else
	{
		Owner->ShowPlayerMessage(LOCTEXT("WrongSlot", "That doesn't go there."));
	}
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOQuestWidgets.h"
#include "UI/MMOItemSlotWidget.h"
#include "UI/MMONPCPlateWidget.h"
#include "UI/MMOUIStyle.h"
#include "MMOCharacter.h"
#include "NPC/MMONPC.h"
#include "Items/MMOItemDefinition.h"
#include "Quests/MMOQuestDefinition.h"
#include "Quests/MMOQuestLogComponent.h"
#include "UI/MMOHUD.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

#define LOCTEXT_NAMESPACE "MMOQuestUI"

namespace MMOQuestUI
{
	static const FLinearColor Body(0.9f, 0.87f, 0.8f);
	static const FLinearColor Done(0.55f, 0.85f, 0.45f);
	static const float WrapWidth = 400.0f;

	UTextBlock* AddParagraph(UWidgetTree* Tree, UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold, float TopPadding)
	{
		UTextBlock* Block = MMOUI::MakeText(Tree, Text, Size, Color, bBold);
		// explicit wrap width: auto-wrap inside an unconstrained box reports the wrong height and overlaps the next line
		Block->SetWrapTextAt(WrapWidth);
		Box->AddChildToVerticalBox(Block)->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
		return Block;
	}

	void AddObjectives(UWidgetTree* Tree, UVerticalBox* Box, const UMMOQuestDefinition* Quest, const UMMOQuestLogComponent* Log)
	{
		AddParagraph(Tree, Box, TEXT("Objectives"), 13, MMOUI::Colors::Gold, true, 8.0f);
		const bool bAccepted = Log && Log->GetQuestState(Quest) >= EMMOQuestState::Active;
		for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
		{
			const FMMOQuestObjective& Objective = Quest->Objectives[i];
			const int32 Have = bAccepted ? Log->GetObjectiveProgress(Quest, i) : 0;
			const bool bDone = bAccepted && Have >= Objective.Count;
			const FString Line = bAccepted
				? FString::Printf(TEXT("- %s: %d/%d"), *Objective.Description.ToString(), Have, Objective.Count)
				: FString::Printf(TEXT("- %s (%d)"), *Objective.Description.ToString(), Objective.Count);
			AddParagraph(Tree, Box, Line, 12, bDone ? Done : Body);
		}
	}

	void AddRewards(UWidgetTree* Tree, UVerticalBox* Box, const UMMOQuestDefinition* Quest)
	{
		AddParagraph(Tree, Box, TEXT("Rewards"), 13, MMOUI::Colors::Gold, true, 8.0f);
		FString Line = FString::Printf(TEXT("%d experience"), Quest->RewardXP);
		if (Quest->RewardCurrency > 0)
		{
			Line += FString::Printf(TEXT("    %s"), *MMOItems::FormatCurrency(Quest->RewardCurrency));
		}
		AddParagraph(Tree, Box, Line, 12, Body);

		if (Quest->RewardItems.Num() > 0)
		{
			UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			for (const FMMOItemReward& Reward : Quest->RewardItems)
			{
				if (!Reward.Item)
				{
					continue;
				}
				UMMOItemSlotWidget* SlotWidget = Tree->ConstructWidget<UMMOItemSlotWidget>(UMMOItemSlotWidget::StaticClass());
				SlotWidget->Configure(EMMOItemSlotContext::Display, INDEX_NONE, EMMOEquipmentSlot::None, FText::GetEmpty(), 44.0f);
				SlotWidget->SetStack(FMMOItemStack::Make(Reward.Item, Reward.Quantity));
				Row->AddChildToHorizontalBox(SlotWidget)->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
				UTextBlock* Name = MMOUI::MakeText(Tree, Reward.Item->DisplayName.ToString(), 12, MMOItems::GetRarityColor(Reward.Item->Rarity), true);
				UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name);
				NameSlot->SetVerticalAlignment(VAlign_Center);
				NameSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
			}
			Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Dialogue

void UMMODialogueWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		UTextBlock* Title = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT(""), 440.0f, Content, CloseWidget, &Title);
		CloseButton = CloseWidget;
		TitleText = Title;
		CloseButton->OnClicked.AddDynamic(this, &UMMODialogueWindowWidget::HandleClose);

		Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(Body);
		Buttons = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Content->AddChildToVerticalBox(Buttons)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
	}
}

UMMOQuestLogComponent* UMMODialogueWindowWidget::GetLog() const
{
	return Character.IsValid() ? Character->GetQuestLog() : nullptr;
}

void UMMODialogueWindowWidget::Open(AMMOCharacter* InCharacter, AMMONPC* InNPC)
{
	Character = InCharacter;
	NPC = InNPC;
	if (TitleText && InNPC)
	{
		const FString Title = InNPC->Title.IsEmpty() ? InNPC->DisplayName.ToString() : FString::Printf(TEXT("%s  -  %s"), *InNPC->DisplayName.ToString(), *InNPC->Title.ToString());
		TitleText->SetText(FText::FromString(Title));
	}
	ShowGreeting();
}

void UMMODialogueWindowWidget::Close()
{
	NPC.Reset();
	ShownQuest.Reset();
}

void UMMODialogueWindowWidget::AddButton(const FString& Label, const FLinearColor& Color, TFunction<void()> OnClick, bool bLeftAligned)
{
	UMMOTextButtonWidget* Button = WidgetTree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
	Button->Setup(Label, Color, 13, bLeftAligned);
	Button->OnClickedNative.BindLambda(MoveTemp(OnClick));
	Buttons->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 2.0f));
}

void UMMODialogueWindowWidget::ShowGreeting()
{
	ShownQuest.Reset();
	if (!Body || !NPC.IsValid())
	{
		return;
	}
	Body->ClearChildren();
	Buttons->ClearChildren();

	MMOQuestUI::AddParagraph(WidgetTree, Body, NPC->Greeting.ToString(), 13, MMOQuestUI::Body);

	UMMOQuestLogComponent* Log = GetLog();
	for (UMMOQuestDefinition* Quest : NPC->GetDialogueQuests(Log))
	{
		const EMMOQuestState State = Log->GetQuestState(Quest);
		const bool bReady = State == EMMOQuestState::ReadyToTurnIn;
		const bool bActive = State == EMMOQuestState::Active;
		const FString Prefix = bReady ? TEXT("?  ") : (bActive ? TEXT("?  ") : TEXT("!  "));
		const FString Suffix = bReady ? TEXT("  (complete)") : (bActive ? TEXT("  (in progress)") : FString());
		const FLinearColor Color = bActive ? FLinearColor(0.7f, 0.7f, 0.7f) : MMOUI::Colors::Gold;
		TWeakObjectPtr<UMMOQuestDefinition> WeakQuest = Quest;
		AddButton(Prefix + Quest->Title.ToString() + Suffix, Color, [this, WeakQuest]() { ShowQuest(WeakQuest.Get()); }, true);
	}

	if (NPC->IsVendor())
	{
		AddButton(TEXT("Let me browse your goods."), FLinearColor(0.75f, 0.9f, 1.0f), [this]()
		{
			APlayerController* PC = Character.IsValid() ? Cast<APlayerController>(Character->GetController()) : nullptr;
			AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr;
			if (HUD && NPC.IsValid())
			{
				HUD->OpenVendor(NPC.Get());
			}
		}, true);
	}

	AddButton(TEXT("Goodbye"), FLinearColor::White, [this]() { HandleClose(); });
}

void UMMODialogueWindowWidget::ShowQuest(UMMOQuestDefinition* Quest)
{
	UMMOQuestLogComponent* Log = GetLog();
	if (!Quest || !Log || !Body)
	{
		ShowGreeting();
		return;
	}
	ShownQuest = Quest;
	Body->ClearChildren();
	Buttons->ClearChildren();

	const EMMOQuestState State = Log->GetQuestState(Quest);
	MMOQuestUI::AddParagraph(WidgetTree, Body, Quest->Title.ToString(), 17, MMOUI::Colors::Gold, true);

	TWeakObjectPtr<UMMOQuestDefinition> WeakQuest = Quest;
	if (State == EMMOQuestState::Available)
	{
		MMOQuestUI::AddParagraph(WidgetTree, Body, Quest->Description.ToString(), 13, MMOQuestUI::Body, false, 6.0f);
		MMOQuestUI::AddObjectives(WidgetTree, Body, Quest, Log);
		MMOQuestUI::AddRewards(WidgetTree, Body, Quest);
		AddButton(TEXT("Accept"), MMOUI::Colors::Gold, [this, WeakQuest]()
		{
			if (UMMOQuestLogComponent* L = GetLog())
			{
				L->AcceptQuest(WeakQuest.Get());
			}
			ShowGreeting();
		});
	}
	else if (State == EMMOQuestState::ReadyToTurnIn)
	{
		MMOQuestUI::AddParagraph(WidgetTree, Body, Quest->CompletionText.ToString(), 13, MMOQuestUI::Body, false, 6.0f);
		MMOQuestUI::AddRewards(WidgetTree, Body, Quest);
		AddButton(TEXT("Complete Quest"), MMOUI::Colors::Gold, [this, WeakQuest]()
		{
			UMMOQuestLogComponent* L = GetLog();
			if (L && L->TurnInQuest(WeakQuest.Get()) == EMMOQuestResult::Success)
			{
				ShowGreeting();
			}
		});
	}
	else
	{
		MMOQuestUI::AddParagraph(WidgetTree, Body, Quest->ProgressText.ToString(), 13, MMOQuestUI::Body, false, 6.0f);
		MMOQuestUI::AddObjectives(WidgetTree, Body, Quest, Log);
	}

	AddButton(TEXT("Back"), FLinearColor::White, [this]() { ShowGreeting(); });
}

void UMMODialogueWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

// ---------------------------------------------------------------------------------------------------------------------
// Quest log

void UMMOQuestLogWindowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UVerticalBox* Content = nullptr;
		UButton* CloseWidget = nullptr;
		WidgetTree->RootWidget = MMOUI::MakeWindow(WidgetTree, TEXT("Quest Log"), 690.0f, Content, CloseWidget);
		CloseButton = CloseWidget;
		CloseButton->OnClicked.AddDynamic(this, &UMMOQuestLogWindowWidget::HandleClose);

		UHorizontalBox* Columns = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Content->AddChildToVerticalBox(Columns);

		USizeBox* ListBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		ListBox->SetWidthOverride(230.0f);
		List = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		ListBox->AddChild(List);
		Columns->AddChildToHorizontalBox(ListBox)->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));

		USizeBox* DetailsBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		DetailsBox->SetWidthOverride(MMOQuestUI::WrapWidth);
		Details = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		DetailsBox->AddChild(Details);
		Columns->AddChildToHorizontalBox(DetailsBox);
	}
}

void UMMOQuestLogWindowWidget::Init(AMMOCharacter* InCharacter)
{
	if (Character.Get() != InCharacter)
	{
		if (Character.IsValid())
		{
			Character->GetQuestLog()->OnQuestLogChanged.RemoveAll(this);
		}
		Character = InCharacter;
		if (InCharacter)
		{
			InCharacter->GetQuestLog()->OnQuestLogChanged.AddDynamic(this, &UMMOQuestLogWindowWidget::Refresh);
		}
	}
	Refresh();
}

void UMMOQuestLogWindowWidget::Refresh()
{
	if (!List || !Character.IsValid())
	{
		return;
	}
	UMMOQuestLogComponent* Log = Character->GetQuestLog();
	List->ClearChildren();
	Details->ClearChildren();

	const TArray<FMMOQuestProgress>& Active = Log->GetActiveQuests();
	if (!Selected.IsValid() || !Active.ContainsByPredicate([this](const FMMOQuestProgress& P) { return P.Quest == Selected.Get(); }))
	{
		Selected = Active.Num() > 0 ? Active[0].Quest.Get() : nullptr;
	}

	for (const FMMOQuestProgress& Progress : Active)
	{
		UMMOQuestDefinition* Quest = Progress.Quest;
		const bool bReady = Log->GetQuestState(Quest) == EMMOQuestState::ReadyToTurnIn;
		UMMOTextButtonWidget* Button = WidgetTree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
		const FString Label = FString::Printf(TEXT("%s[%d] %s%s"), Quest == Selected.Get() ? TEXT("> ") : TEXT(""), Quest->RecommendedLevel, *Quest->Title.ToString(), bReady ? TEXT("  (complete)") : TEXT(""));
		Button->Setup(Label, bReady ? MMOUI::Colors::Gold : FLinearColor::White, 12, true);
		TWeakObjectPtr<UMMOQuestDefinition> WeakQuest = Quest;
		Button->OnClickedNative.BindLambda([this, WeakQuest]() { Selected = WeakQuest; Refresh(); });
		List->AddChildToVerticalBox(Button)->SetPadding(FMargin(0.0f, 2.0f));
	}

	UMMOQuestDefinition* Quest = Selected.Get();
	if (!Quest)
	{
		MMOQuestUI::AddParagraph(WidgetTree, Details, TEXT("You have no quests. Look for townsfolk with a gold ! over their heads."), 13, MMOQuestUI::Body);
		return;
	}

	MMOQuestUI::AddParagraph(WidgetTree, Details, Quest->Title.ToString(), 17, MMOUI::Colors::Gold, true);
	MMOQuestUI::AddParagraph(WidgetTree, Details, Quest->Summary.ToString(), 13, MMOQuestUI::Body, false, 6.0f);
	MMOQuestUI::AddObjectives(WidgetTree, Details, Quest, Log);
	MMOQuestUI::AddRewards(WidgetTree, Details, Quest);

	UMMOTextButtonWidget* Abandon = WidgetTree->ConstructWidget<UMMOTextButtonWidget>(UMMOTextButtonWidget::StaticClass());
	Abandon->Setup(TEXT("Abandon Quest"), MMOUI::Colors::Error, 12);
	TWeakObjectPtr<UMMOQuestDefinition> WeakQuest = Quest;
	Abandon->OnClickedNative.BindLambda([this, WeakQuest]()
	{
		if (Character.IsValid())
		{
			Character->GetQuestLog()->AbandonQuest(WeakQuest.Get());
		}
	});
	Details->AddChildToVerticalBox(Abandon)->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
}

void UMMOQuestLogWindowWidget::HandleClose()
{
	OnCloseRequested.ExecuteIfBound();
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Quests/MMOQuestLogComponent.h"
#include "Quests/MMOQuestDefinition.h"
#include "Combat/MMOProgressionComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "World/MMODiscoveryZone.h"
#include "World/MMOExplorationComponent.h"
#include "MMO.h"

#define LOCTEXT_NAMESPACE "MMOQuests"

UMMOQuestLogComponent::UMMOQuestLogComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMMOQuestLogComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		BindSources(Owner->FindComponentByClass<UMMOInventoryComponent>());
		if (UMMOExplorationComponent* Exploration = Owner->FindComponentByClass<UMMOExplorationComponent>())
		{
			Exploration->OnLocationDiscovered.AddDynamic(this, &UMMOQuestLogComponent::HandleLocationDiscovered);
		}
	}
}

void UMMOQuestLogComponent::BindSources(UMMOInventoryComponent* InInventory)
{
	if (Inventory)
	{
		Inventory->OnInventoryChanged.RemoveAll(this);
	}
	Inventory = InInventory;
	if (Inventory)
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UMMOQuestLogComponent::HandleInventoryChanged);
	}
}

FMMOQuestProgress* UMMOQuestLogComponent::FindActive(const UMMOQuestDefinition* Quest)
{
	return Active.FindByPredicate([Quest](const FMMOQuestProgress& P) { return P.Quest == Quest; });
}

const FMMOQuestProgress* UMMOQuestLogComponent::FindActive(const UMMOQuestDefinition* Quest) const
{
	return Active.FindByPredicate([Quest](const FMMOQuestProgress& P) { return P.Quest == Quest; });
}

bool UMMOQuestLogComponent::IsDiscovered(FName LocationId) const
{
	const UMMOExplorationComponent* Exploration = GetOwner() ? GetOwner()->FindComponentByClass<UMMOExplorationComponent>() : nullptr;
	return Exploration && Exploration->HasDiscovered(LocationId);
}

EMMOQuestState UMMOQuestLogComponent::GetQuestState(const UMMOQuestDefinition* Quest) const
{
	if (!Quest)
	{
		return EMMOQuestState::Unavailable;
	}
	if (Completed.Contains(Quest->QuestId))
	{
		return EMMOQuestState::Completed;
	}
	if (FindActive(Quest))
	{
		return AreObjectivesComplete(Quest) ? EMMOQuestState::ReadyToTurnIn : EMMOQuestState::Active;
	}
	if (Quest->Prerequisite && !Completed.Contains(Quest->Prerequisite->QuestId))
	{
		return EMMOQuestState::Unavailable;
	}
	return EMMOQuestState::Available;
}

EMMOQuestResult UMMOQuestLogComponent::AcceptQuest(UMMOQuestDefinition* Quest)
{
	if (GetQuestState(Quest) != EMMOQuestState::Available)
	{
		return EMMOQuestResult::NotAvailable;
	}
	if (Active.Num() >= MaxActiveQuests)
	{
		Message(LOCTEXT("LogFull", "Your quest log is full."), true);
		return EMMOQuestResult::LogFull;
	}

	FMMOQuestProgress& Progress = Active.AddDefaulted_GetRef();
	Progress.Quest = Quest;
	Progress.Counts.Init(0, Quest->Objectives.Num());

	// places already visited count straight away
	for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
	{
		const FMMOQuestObjective& Objective = Quest->Objectives[i];
		if (Objective.Type == EMMOQuestObjectiveType::Discover && IsDiscovered(Objective.TargetId))
		{
			Progress.Counts[i] = Objective.Count;
		}
		if (Objective.Type == EMMOQuestObjectiveType::Collect)
		{
			LastCollectCounts.Add(Objective.TargetId, GetObjectiveProgress(Quest, i));
		}
	}

	Message(FText::Format(LOCTEXT("Accepted", "Quest accepted: {0}"), Quest->Title), true);
	if (AreObjectivesComplete(Quest))
	{
		Message(FText::Format(LOCTEXT("CompleteNow", "{0} complete - return to turn it in"), Quest->Title), true);
	}
	OnQuestLogChanged.Broadcast();
	return EMMOQuestResult::Success;
}

bool UMMOQuestLogComponent::AbandonQuest(const UMMOQuestDefinition* Quest)
{
	const int32 Removed = Active.RemoveAll([Quest](const FMMOQuestProgress& P) { return P.Quest == Quest; });
	if (Removed > 0)
	{
		Message(FText::Format(LOCTEXT("Abandoned", "Quest abandoned: {0}"), Quest->Title));
		OnQuestLogChanged.Broadcast();
	}
	return Removed > 0;
}

int32 UMMOQuestLogComponent::GetObjectiveProgress(const UMMOQuestDefinition* Quest, int32 ObjectiveIndex) const
{
	if (!Quest || !Quest->Objectives.IsValidIndex(ObjectiveIndex))
	{
		return 0;
	}

	const FMMOQuestObjective& Objective = Quest->Objectives[ObjectiveIndex];
	if (Completed.Contains(Quest->QuestId))
	{
		return Objective.Count;
	}

	if (Objective.Type == EMMOQuestObjectiveType::Collect)
	{
		const UMMOItemDefinition* Item = UMMOItemDefinition::FindById(Objective.TargetId);
		return Inventory && Item ? FMath::Min(Inventory->CountItem(Item), Objective.Count) : 0;
	}

	const FMMOQuestProgress* Progress = FindActive(Quest);
	return Progress && Progress->Counts.IsValidIndex(ObjectiveIndex) ? FMath::Min(Progress->Counts[ObjectiveIndex], Objective.Count) : 0;
}

bool UMMOQuestLogComponent::AreObjectivesComplete(const UMMOQuestDefinition* Quest) const
{
	if (!Quest)
	{
		return false;
	}
	for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
	{
		if (GetObjectiveProgress(Quest, i) < Quest->Objectives[i].Count)
		{
			return false;
		}
	}
	return true;
}

void UMMOQuestLogComponent::AnnounceProgress(const UMMOQuestDefinition* Quest, int32 ObjectiveIndex, bool bWasComplete)
{
	const FMMOQuestObjective& Objective = Quest->Objectives[ObjectiveIndex];
	Message(FText::Format(LOCTEXT("Progress", "{0}: {1}/{2}"), Objective.Description, GetObjectiveProgress(Quest, ObjectiveIndex), Objective.Count));
	if (!bWasComplete && AreObjectivesComplete(Quest))
	{
		Message(FText::Format(LOCTEXT("Complete", "{0} complete - return to turn it in"), Quest->Title), true);
	}
	OnQuestLogChanged.Broadcast();
}

void UMMOQuestLogComponent::NotifyKill(FName QuestTag)
{
	if (QuestTag.IsNone())
	{
		return;
	}

	for (FMMOQuestProgress& Progress : Active)
	{
		const UMMOQuestDefinition* Quest = Progress.Quest;
		for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
		{
			const FMMOQuestObjective& Objective = Quest->Objectives[i];
			if (Objective.Type == EMMOQuestObjectiveType::Kill && Objective.TargetId == QuestTag && Progress.Counts[i] < Objective.Count)
			{
				const bool bWasComplete = AreObjectivesComplete(Quest);
				++Progress.Counts[i];
				AnnounceProgress(Quest, i, bWasComplete);
			}
		}
	}
}

void UMMOQuestLogComponent::NotifyDiscovered(FName LocationId)
{
	for (FMMOQuestProgress& Progress : Active)
	{
		const UMMOQuestDefinition* Quest = Progress.Quest;
		for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
		{
			const FMMOQuestObjective& Objective = Quest->Objectives[i];
			if (Objective.Type == EMMOQuestObjectiveType::Discover && Objective.TargetId == LocationId && Progress.Counts[i] < Objective.Count)
			{
				const bool bWasComplete = AreObjectivesComplete(Quest);
				Progress.Counts[i] = Objective.Count;
				AnnounceProgress(Quest, i, bWasComplete);
			}
		}
	}
}

void UMMOQuestLogComponent::HandleLocationDiscovered(AMMODiscoveryZone* Zone, int32 XPAwarded)
{
	if (Zone)
	{
		NotifyDiscovered(Zone->LocationId);
	}
}

void UMMOQuestLogComponent::HandleInventoryChanged()
{
	// announce collect progress when the relevant item counts change
	for (const FMMOQuestProgress& Progress : Active)
	{
		const UMMOQuestDefinition* Quest = Progress.Quest;
		for (int32 i = 0; i < Quest->Objectives.Num(); ++i)
		{
			const FMMOQuestObjective& Objective = Quest->Objectives[i];
			if (Objective.Type != EMMOQuestObjectiveType::Collect)
			{
				continue;
			}
			const int32 Now = GetObjectiveProgress(Quest, i);
			int32& Last = LastCollectCounts.FindOrAdd(Objective.TargetId, 0);
			if (Now != Last)
			{
				const bool bWasComplete = Last >= Objective.Count;
				Last = Now;
				AnnounceProgress(Quest, i, bWasComplete);
			}
		}
	}
}

EMMOQuestResult UMMOQuestLogComponent::TurnInQuest(UMMOQuestDefinition* Quest)
{
	if (GetQuestState(Quest) != EMMOQuestState::ReadyToTurnIn)
	{
		return EMMOQuestResult::NotReady;
	}

	// reward items must fit before anything changes
	if (Inventory)
	{
		int32 SlotsNeeded = 0;
		for (const FMMOItemReward& Reward : Quest->RewardItems)
		{
			if (Reward.Item && Inventory->GetAddableQuantity(Reward.Item, Reward.Quantity) < Reward.Quantity)
			{
				++SlotsNeeded;
			}
		}
		if (SlotsNeeded > 0)
		{
			Message(LOCTEXT("BagFull", "Inventory Full - make room for the reward"), true);
			return EMMOQuestResult::InventoryFull;
		}
	}

	// complete first, so handing over items doesn't announce "0/4" progress
	Active.RemoveAll([Quest](const FMMOQuestProgress& P) { return P.Quest == Quest; });
	Completed.Add(Quest->QuestId);

	// hand over collected items
	if (Inventory)
	{
		for (const FMMOQuestObjective& Objective : Quest->Objectives)
		{
			if (Objective.Type == EMMOQuestObjectiveType::Collect)
			{
				Inventory->RemoveItem(UMMOItemDefinition::FindById(Objective.TargetId), Objective.Count);
			}
		}
	}

	if (Inventory)
	{
		for (const FMMOItemReward& Reward : Quest->RewardItems)
		{
			if (Reward.Item)
			{
				Inventory->AddItem(Reward.Item, Reward.Quantity, true);
			}
		}
		Inventory->AddCurrency(Quest->RewardCurrency, true);
	}
	if (UMMOProgressionComponent* Progression = GetOwner() ? GetOwner()->FindComponentByClass<UMMOProgressionComponent>() : nullptr)
	{
		Progression->AddXP(Quest->RewardXP);
	}

	UE_LOG(LogMMO, Log, TEXT("Quest turned in: %s (+%d XP)"), *Quest->Title.ToString(), Quest->RewardXP);
	Message(FText::Format(LOCTEXT("TurnedIn", "Quest completed: {0}"), Quest->Title), true);
	OnQuestLogChanged.Broadcast();
	return EMMOQuestResult::Success;
}

void UMMOQuestLogComponent::RestoreState(const TArray<FMMOQuestProgress>& InActive, const TSet<FName>& InCompleted)
{
	Active.Reset();
	for (const FMMOQuestProgress& Progress : InActive)
	{
		if (Progress.Quest)
		{
			FMMOQuestProgress& Copy = Active.Add_GetRef(Progress);
			Copy.Counts.SetNum(Progress.Quest->Objectives.Num());
		}
	}
	Completed = InCompleted;
	LastCollectCounts.Reset();
	OnQuestLogChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE

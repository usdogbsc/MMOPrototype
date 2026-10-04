// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Quests/MMOQuestDefinition.h"
#include "Quests/MMOQuestLogComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MMOQuestTestUtils
{
	static UMMOItemDefinition* MakeItem(FName Id, int32 MaxStack)
	{
		UMMOItemDefinition* Item = NewObject<UMMOItemDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Item->ItemId = Id;
		Item->DisplayName = FText::FromName(Id);
		Item->MaxStackSize = MaxStack;
		return Item;
	}

	static UMMOQuestDefinition* MakeQuest(FName Id, EMMOQuestObjectiveType Type, FName Target, int32 Count, UMMOQuestDefinition* Prerequisite = nullptr)
	{
		UMMOQuestDefinition* Quest = NewObject<UMMOQuestDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Quest->QuestId = Id;
		Quest->Title = FText::FromName(Id);
		Quest->Prerequisite = Prerequisite;
		FMMOQuestObjective& Objective = Quest->Objectives.AddDefaulted_GetRef();
		Objective.Type = Type;
		Objective.TargetId = Target;
		Objective.Count = Count;
		Objective.Description = FText::FromName(Target);
		Quest->RewardXP = 100;
		Quest->RewardCurrency = 25;
		return Quest;
	}

	static UMMOInventoryComponent* MakeInventory(int32 Capacity)
	{
		UMMOInventoryComponent* Inventory = NewObject<UMMOInventoryComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Inventory->Capacity = Capacity;
		Inventory->EnsureSlots();
		return Inventory;
	}

	static UMMOQuestLogComponent* MakeLog(UMMOInventoryComponent* Inventory)
	{
		UMMOQuestLogComponent* Log = NewObject<UMMOQuestLogComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Log->BindSources(Inventory);
		return Log;
	}
}

namespace QT = MMOQuestTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOQuestKillTest, "MMO.Quests.KillObjectiveAndPrerequisite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOQuestKillTest::RunTest(const FString& Parameters)
{
	UMMOQuestDefinition* First = QT::MakeQuest(TEXT("QT_KillFirst"), EMMOQuestObjectiveType::Kill, TEXT("QT_Wolf"), 3);
	UMMOQuestDefinition* Second = QT::MakeQuest(TEXT("QT_KillSecond"), EMMOQuestObjectiveType::Kill, TEXT("QT_Wolf"), 1, First);
	UMMOQuestLogComponent* Log = QT::MakeLog(QT::MakeInventory(4));

	TestEqual(TEXT("First quest available"), Log->GetQuestState(First), EMMOQuestState::Available);
	TestEqual(TEXT("Follow-up locked behind prerequisite"), Log->GetQuestState(Second), EMMOQuestState::Unavailable);
	TestEqual(TEXT("Cannot accept a locked quest"), Log->AcceptQuest(Second), EMMOQuestResult::NotAvailable);

	TestEqual(TEXT("Accept"), Log->AcceptQuest(First), EMMOQuestResult::Success);
	TestEqual(TEXT("Accepting twice fails"), Log->AcceptQuest(First), EMMOQuestResult::NotAvailable);
	TestEqual(TEXT("Active"), Log->GetQuestState(First), EMMOQuestState::Active);

	Log->NotifyKill(TEXT("QT_Bear"));
	TestEqual(TEXT("Other creatures don't count"), Log->GetObjectiveProgress(First, 0), 0);
	Log->NotifyKill(TEXT("QT_Wolf"));
	Log->NotifyKill(TEXT("QT_Wolf"));
	TestEqual(TEXT("Two kills counted"), Log->GetObjectiveProgress(First, 0), 2);
	TestEqual(TEXT("Not ready at 2/3"), Log->GetQuestState(First), EMMOQuestState::Active);
	TestEqual(TEXT("Turn-in refused before completion"), Log->TurnInQuest(First), EMMOQuestResult::NotReady);

	Log->NotifyKill(TEXT("QT_Wolf"));
	Log->NotifyKill(TEXT("QT_Wolf"));
	TestEqual(TEXT("Progress capped at the count"), Log->GetObjectiveProgress(First, 0), 3);
	TestEqual(TEXT("Ready to turn in"), Log->GetQuestState(First), EMMOQuestState::ReadyToTurnIn);

	TestEqual(TEXT("Turn in"), Log->TurnInQuest(First), EMMOQuestResult::Success);
	TestEqual(TEXT("Completed"), Log->GetQuestState(First), EMMOQuestState::Completed);
	TestTrue(TEXT("Recorded as completed"), Log->HasCompleted(TEXT("QT_KillFirst")));
	TestEqual(TEXT("Removed from the log"), Log->GetActiveQuests().Num(), 0);
	TestEqual(TEXT("Cannot re-accept a completed quest"), Log->AcceptQuest(First), EMMOQuestResult::NotAvailable);
	TestEqual(TEXT("Follow-up unlocked"), Log->GetQuestState(Second), EMMOQuestState::Available);

	// kills made before accepting don't count
	TestEqual(TEXT("Accept follow-up"), Log->AcceptQuest(Second), EMMOQuestResult::Success);
	TestEqual(TEXT("Starts at zero"), Log->GetObjectiveProgress(Second, 0), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOQuestCollectTest, "MMO.Quests.CollectAndRewards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOQuestCollectTest::RunTest(const FString& Parameters)
{
	UMMOItemDefinition* Pelt = QT::MakeItem(TEXT("QT_CollectPelt"), 20);
	UMMOItemDefinition* Gloves = QT::MakeItem(TEXT("QT_CollectGloves"), 1);
	UMMOItemDefinition* Junk = QT::MakeItem(TEXT("QT_CollectJunk"), 1);
	UMMOQuestDefinition* Quest = QT::MakeQuest(TEXT("QT_Collect"), EMMOQuestObjectiveType::Collect, TEXT("QT_CollectPelt"), 4);
	Quest->RewardItems.Add({ Gloves, 1 });

	UMMOInventoryComponent* Inventory = QT::MakeInventory(3);
	UMMOQuestLogComponent* Log = QT::MakeLog(Inventory);

	Inventory->AddItem(Pelt, 2);
	TestEqual(TEXT("Accept"), Log->AcceptQuest(Quest), EMMOQuestResult::Success);
	TestEqual(TEXT("Items already carried count"), Log->GetObjectiveProgress(Quest, 0), 2);

	Inventory->AddItem(Pelt, 3);
	TestEqual(TEXT("Counts live from the backpack (capped)"), Log->GetObjectiveProgress(Quest, 0), 4);
	TestEqual(TEXT("Ready"), Log->GetQuestState(Quest), EMMOQuestState::ReadyToTurnIn);

	Inventory->RemoveItem(Pelt, 2);
	TestEqual(TEXT("Dropping items lowers progress"), Log->GetObjectiveProgress(Quest, 0), 3);
	TestEqual(TEXT("No longer ready"), Log->GetQuestState(Quest), EMMOQuestState::Active);
	Inventory->AddItem(Pelt, 1);

	// a full backpack blocks the reward without taking anything
	Inventory->AddItem(Junk, 1);
	Inventory->AddItem(Junk, 1);
	TestEqual(TEXT("Bag full"), Inventory->GetAddableQuantity(Gloves, 1), 0);
	TestEqual(TEXT("Turn-in blocked by full bag"), Log->TurnInQuest(Quest), EMMOQuestResult::InventoryFull);
	TestEqual(TEXT("Pelts kept"), Inventory->CountItem(Pelt), 4);
	TestEqual(TEXT("Still ready"), Log->GetQuestState(Quest), EMMOQuestState::ReadyToTurnIn);

	Inventory->RemoveItem(Junk, 1);
	TestEqual(TEXT("Turn in"), Log->TurnInQuest(Quest), EMMOQuestResult::Success);
	TestEqual(TEXT("Pelts handed over"), Inventory->CountItem(Pelt), 0);
	TestEqual(TEXT("Reward item received"), Inventory->CountItem(Gloves), 1);
	TestEqual(TEXT("Reward currency received"), Inventory->GetCurrency(), 25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOQuestDiscoverAbandonRestoreTest, "MMO.Quests.DiscoverAbandonRestore",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOQuestDiscoverAbandonRestoreTest::RunTest(const FString& Parameters)
{
	UMMOQuestDefinition* Scout = QT::MakeQuest(TEXT("QT_Scout"), EMMOQuestObjectiveType::Discover, TEXT("QT_Tower"), 1);
	FMMOQuestObjective& Second = Scout->Objectives.AddDefaulted_GetRef();
	Second.Type = EMMOQuestObjectiveType::Discover;
	Second.TargetId = TEXT("QT_Woods");
	Second.Count = 1;
	UMMOQuestDefinition* Hunt = QT::MakeQuest(TEXT("QT_Hunt"), EMMOQuestObjectiveType::Kill, TEXT("QT_Boar"), 5);

	UMMOQuestLogComponent* Log = QT::MakeLog(QT::MakeInventory(2));
	Log->AcceptQuest(Scout);
	Log->NotifyDiscovered(TEXT("QT_Tower"));
	TestEqual(TEXT("First place scouted"), Log->GetObjectiveProgress(Scout, 0), 1);
	TestEqual(TEXT("Second not yet"), Log->GetQuestState(Scout), EMMOQuestState::Active);
	Log->NotifyDiscovered(TEXT("QT_Woods"));
	TestEqual(TEXT("Both scouted"), Log->GetQuestState(Scout), EMMOQuestState::ReadyToTurnIn);

	// abandoning clears progress; the quest can be picked up again
	Log->AcceptQuest(Hunt);
	Log->NotifyKill(TEXT("QT_Boar"));
	TestTrue(TEXT("Abandon"), Log->AbandonQuest(Hunt));
	TestEqual(TEXT("Available again"), Log->GetQuestState(Hunt), EMMOQuestState::Available);
	Log->AcceptQuest(Hunt);
	TestEqual(TEXT("Progress reset"), Log->GetObjectiveProgress(Hunt, 0), 0);

	// restoring state (save games) replaces everything
	FMMOQuestProgress Saved;
	Saved.Quest = Hunt;
	Saved.Counts = { 4 };
	TSet<FName> Done = { TEXT("QT_Scout") };
	Log->RestoreState({ Saved }, Done);
	TestEqual(TEXT("Restored progress"), Log->GetObjectiveProgress(Hunt, 0), 4);
	TestEqual(TEXT("Restored completion"), Log->GetQuestState(Scout), EMMOQuestState::Completed);
	TestEqual(TEXT("One active quest"), Log->GetActiveQuests().Num(), 1);

	// the log has a size limit
	Log->MaxActiveQuests = 1;
	UMMOQuestDefinition* Extra = QT::MakeQuest(TEXT("QT_Extra"), EMMOQuestObjectiveType::Kill, TEXT("QT_Boar"), 1);
	TestEqual(TEXT("Log full"), Log->AcceptQuest(Extra), EMMOQuestResult::LogFull);
	return true;
}

#endif

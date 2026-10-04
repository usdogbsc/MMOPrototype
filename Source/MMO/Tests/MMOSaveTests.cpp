// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Combat/MMOProgressionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Save/MMOSaveGame.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOSaveGameRoundTripTest, "MMO.Save.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOSaveGameRoundTripTest::RunTest(const FString& Parameters)
{
	UMMOSaveGame* Save = NewObject<UMMOSaveGame>(GetTransientPackage(), NAME_None, RF_Transient);
	Save->Level = 4;
	Save->XP = 120;
	Save->Health = 87.5f;
	Save->MapName = TEXT("/Game/MMO/Maps/Lvl_Thornwick");
	Save->bHasLocation = true;
	Save->Location = FVector(1200.0f, -340.0f, 95.0f);
	Save->Yaw = 135.0f;
	Save->Currency = 1234;
	Save->Inventory = { { TEXT("WolfPelt"), 7, 0 }, { TEXT("WolfFang"), 3, 5 } };
	Save->Equipment = { { TEXT("Greyfang"), 1, static_cast<int32>(EMMOEquipmentSlot::MainHand) } };
	Save->ActiveQuests = { { TEXT("WolvesAtTheGate"), { 3 } } };
	Save->CompletedQuests = { TEXT("PeltsForTheHearth") };
	Save->Discovered = { TEXT("Thornwick"), TEXT("Greywood") };

	TArray<uint8> Bytes;
	TestTrue(TEXT("Serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes) && Bytes.Num() > 0);
	const UMMOSaveGame* Loaded = Cast<UMMOSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("Deserializes as UMMOSaveGame"), Loaded))
	{
		return false;
	}

	TestEqual(TEXT("Version"), Loaded->Version, UMMOSaveGame::CurrentVersion);
	TestEqual(TEXT("Level"), Loaded->Level, 4);
	TestEqual(TEXT("XP"), Loaded->XP, 120);
	TestEqual(TEXT("Health"), Loaded->Health, 87.5f);
	TestEqual(TEXT("Map"), Loaded->MapName, Save->MapName);
	TestTrue(TEXT("Location"), Loaded->bHasLocation && Loaded->Location.Equals(Save->Location));
	TestEqual(TEXT("Currency"), Loaded->Currency, 1234);
	TestTrue(TEXT("Inventory"), Loaded->Inventory.Num() == 2 && Loaded->Inventory[1].ItemId == TEXT("WolfFang") && Loaded->Inventory[1].Quantity == 3 && Loaded->Inventory[1].Slot == 5);
	TestTrue(TEXT("Equipment"), Loaded->Equipment.Num() == 1 && Loaded->Equipment[0].ItemId == TEXT("Greyfang"));
	TestTrue(TEXT("Quests"), Loaded->ActiveQuests.Num() == 1 && Loaded->ActiveQuests[0].Counts == TArray<int32>{ 3 } && Loaded->CompletedQuests == TArray<FName>{ TEXT("PeltsForTheHearth") });
	TestEqual(TEXT("Discovered"), Loaded->Discovered.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOProgressionRestoreTest, "MMO.Save.ProgressionRestoreClamps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOProgressionRestoreTest::RunTest(const FString& Parameters)
{
	UMMOProgressionComponent* Progression = NewObject<UMMOProgressionComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	Progression->MaxLevel = 20;

	Progression->RestoreProgress(3, 50);
	TestEqual(TEXT("Level restored"), Progression->GetLevel(), 3);
	TestEqual(TEXT("XP restored"), Progression->GetCurrentXP(), 50);

	Progression->RestoreProgress(3, 100000);
	TestTrue(TEXT("XP clamped below the next level"), Progression->GetCurrentXP() < Progression->GetXPToNextLevel());

	Progression->RestoreProgress(99, 10);
	TestEqual(TEXT("Level clamped to the cap"), Progression->GetLevel(), 20);
	TestEqual(TEXT("No XP at the cap"), Progression->GetCurrentXP(), 0);

	Progression->RestoreProgress(-5, -5);
	TestEqual(TEXT("Level at least 1"), Progression->GetLevel(), 1);
	TestEqual(TEXT("XP at least 0"), Progression->GetCurrentXP(), 0);
	return true;
}

#endif

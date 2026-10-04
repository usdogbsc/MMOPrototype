// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Combat/MMOHealthComponent.h"
#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOLootContainerComponent.h"
#include "Items/MMOLootTable.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MMOItemTestUtils
{
	static UMMOItemDefinition* MakeItem(FName Id, int32 MaxStack, EMMOEquipmentSlot Slot = EMMOEquipmentSlot::None, float DamageMin = 0.0f, float DamageMax = 0.0f, float Armor = 0.0f)
	{
		UMMOItemDefinition* Item = NewObject<UMMOItemDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Item->ItemId = Id;
		Item->DisplayName = FText::FromName(Id);
		Item->MaxStackSize = MaxStack;
		Item->EquipmentSlot = Slot;
		Item->WeaponDamageMin = DamageMin;
		Item->WeaponDamageMax = DamageMax;
		Item->Stats.Armor = Armor;
		return Item;
	}

	static UMMOInventoryComponent* MakeInventory(int32 Capacity)
	{
		UMMOInventoryComponent* Inventory = NewObject<UMMOInventoryComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Inventory->Capacity = Capacity;
		Inventory->EnsureSlots();
		return Inventory;
	}

	static UMMOEquipmentComponent* MakeEquipment()
	{
		UMMOEquipmentComponent* Equipment = NewObject<UMMOEquipmentComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Equipment->StartingEquipment.Reset();
		Equipment->EnsureSlots();
		return Equipment;
	}

	static UMMOLootContainerComponent* MakeContainer()
	{
		return NewObject<UMMOLootContainerComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	}
}

using namespace MMOItemTestUtils;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOInventoryStackingTest, "MMO.Items.Inventory.Stacking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOInventoryStackingTest::RunTest(const FString& Parameters)
{
	UMMOItemDefinition* Pelt = MakeItem(TEXT("Pelt"), 20);
	UMMOInventoryComponent* Inventory = MakeInventory(4);

	TestEqual(TEXT("Adds 3 pelts"), Inventory->AddItem(Pelt, 3), 3);
	TestEqual(TEXT("Adds 2 more"), Inventory->AddItem(Pelt, 2), 2);
	TestEqual(TEXT("Merged into one stack of 5"), Inventory->GetSlot(0).Quantity, 5);
	TestTrue(TEXT("Second slot still empty"), Inventory->GetSlot(1).IsEmpty());

	// overflow past max stack starts a new stack
	TestEqual(TEXT("Adds 18"), Inventory->AddItem(Pelt, 18), 18);
	TestEqual(TEXT("First stack capped at 20"), Inventory->GetSlot(0).Quantity, 20);
	TestEqual(TEXT("Overflow in second stack"), Inventory->GetSlot(1).Quantity, 3);
	TestEqual(TEXT("Total pelts"), Inventory->CountItem(Pelt), 23);

	// moving a stack onto a matching stack merges it
	Inventory->MoveSlot(1, 2);
	TestEqual(TEXT("Move to empty slot"), Inventory->GetSlot(2).Quantity, 3);
	Inventory->AddItem(Pelt, 0);
	TestEqual(TEXT("Zero quantity ignored"), Inventory->CountItem(Pelt), 23);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOInventoryCapacityTest, "MMO.Items.Inventory.CapacityAndFull",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOInventoryCapacityTest::RunTest(const FString& Parameters)
{
	UMMOItemDefinition* Pelt = MakeItem(TEXT("Pelt"), 20);
	UMMOItemDefinition* Boots = MakeItem(TEXT("Boots"), 1, EMMOEquipmentSlot::Feet, 0, 0, 8);
	UMMOInventoryComponent* Inventory = MakeInventory(3);

	TestEqual(TEXT("Non-stackables use one slot each"), Inventory->AddItem(Boots, 2), 2);
	TestEqual(TEXT("One slot left"), Inventory->GetFreeSlotCount(), 1);
	TestEqual(TEXT("Addable is capped by room"), Inventory->GetAddableQuantity(Pelt, 50), 20);
	TestEqual(TEXT("Only 20 pelts fit"), Inventory->AddItem(Pelt, 50), 20);
	TestEqual(TEXT("Inventory full"), Inventory->GetFreeSlotCount(), 0);
	TestEqual(TEXT("Nothing more fits"), Inventory->AddItem(Pelt, 1), 0);
	TestEqual(TEXT("No boots fit"), Inventory->AddItem(Boots, 1), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOLootTransferTest, "MMO.Items.Loot.TransferAndLootAll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOLootTransferTest::RunTest(const FString& Parameters)
{
	UMMOItemDefinition* Pelt = MakeItem(TEXT("Pelt"), 20);
	UMMOItemDefinition* Fang = MakeItem(TEXT("Fang"), 20);
	UMMOItemDefinition* Sword = MakeItem(TEXT("Sword"), 1, EMMOEquipmentSlot::MainHand, 17, 22);
	UMMOInventoryComponent* Inventory = MakeInventory(4);
	UMMOLootContainerComponent* Corpse = MakeContainer();

	Inventory->AddItem(Pelt, 3);
	Corpse->SetLoot({ FMMOItemStack::Make(Pelt, 2), FMMOItemStack::Make(Fang, 1), FMMOItemStack::Make(Sword, 1) }, 7);

	// loot one item
	const FGuid PeltId = Corpse->GetItems()[0].InstanceId;
	TestEqual(TEXT("Loot single item"), Corpse->TakeItem(PeltId, Inventory), EMMOLootResult::Success);
	TestEqual(TEXT("Pelts stacked to 5"), Inventory->CountItem(Pelt), 5);
	TestEqual(TEXT("Removed from corpse"), Corpse->GetItems().Num(), 2);

	// repeated clicks on the same stack never duplicate it
	TestEqual(TEXT("Second click finds nothing"), Corpse->TakeItem(PeltId, Inventory), EMMOLootResult::NotFound);
	TestEqual(TEXT("Still 5 pelts"), Inventory->CountItem(Pelt), 5);

	// loot all, keeping the sword's identity
	const FGuid SwordId = Corpse->GetItems()[1].InstanceId;
	TestEqual(TEXT("Loot All succeeds"), Corpse->TakeAll(Inventory), EMMOLootResult::Success);
	TestFalse(TEXT("Corpse empty"), Corpse->HasLoot());
	TestEqual(TEXT("Currency taken"), Inventory->GetCurrency(), 7);
	TestEqual(TEXT("Fang taken"), Inventory->CountItem(Fang), 1);
	TestTrue(TEXT("Sword keeps its instance id"), Inventory->GetSlots().ContainsByPredicate([&SwordId](const FMMOItemStack& S) { return S.InstanceId == SwordId; }));

	// rapid Loot All on an empty corpse does nothing
	TestEqual(TEXT("Loot All on empty corpse"), Corpse->TakeAll(Inventory), EMMOLootResult::NotFound);
	TestEqual(TEXT("Inventory total unchanged"), Inventory->GetTotalItemCount(), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOLootFullInventoryTest, "MMO.Items.Loot.FullInventoryKeepsLoot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOLootFullInventoryTest::RunTest(const FString& Parameters)
{
	UMMOItemDefinition* Pelt = MakeItem(TEXT("Pelt"), 20);
	UMMOItemDefinition* Fang = MakeItem(TEXT("Fang"), 20);
	UMMOItemDefinition* Boots = MakeItem(TEXT("Boots"), 1, EMMOEquipmentSlot::Feet);
	UMMOInventoryComponent* Inventory = MakeInventory(2);
	UMMOLootContainerComponent* Corpse = MakeContainer();

	Inventory->AddItem(Fang, 20);
	Inventory->AddItem(Pelt, 18);
	Corpse->SetLoot({ FMMOItemStack::Make(Pelt, 5), FMMOItemStack::Make(Boots, 1) }, 0);

	// only 2 pelts fit; boots don't fit at all
	TestEqual(TEXT("Loot All is partial"), Corpse->TakeAll(Inventory), EMMOLootResult::Partial);
	TestEqual(TEXT("Pelt stack topped up"), Inventory->CountItem(Pelt), 20);
	TestEqual(TEXT("3 pelts stay on the corpse"), Corpse->GetItems().Num() > 0 ? Corpse->GetItems()[0].Quantity : 0, 3);
	TestEqual(TEXT("Boots stay on the corpse"), Corpse->GetTotalItemCount(), 4);

	const FGuid BootsId = Corpse->GetItems()[1].InstanceId;
	TestEqual(TEXT("Looting boots reports full"), Corpse->TakeItem(BootsId, Inventory), EMMOLootResult::InventoryFull);
	TestEqual(TEXT("Nothing was lost"), Inventory->GetTotalItemCount() + Corpse->GetTotalItemCount(), 20 + 18 + 5 + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOLootIsolationTest, "MMO.Items.Loot.CorpseIsolationAndTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOLootIsolationTest::RunTest(const FString& Parameters)
{
	UMMOItemDefinition* Pelt = MakeItem(TEXT("Pelt"), 20);
	UMMOItemDefinition* Sword = MakeItem(TEXT("Sword"), 1, EMMOEquipmentSlot::MainHand, 17, 22);

	UMMOLootTable* Table = NewObject<UMMOLootTable>(GetTransientPackage(), NAME_None, RF_Transient);
	FMMOLootEntry Always;
	Always.Item = Pelt;
	Always.DropChance = 1.0f;
	Table->Entries.Add(Always);
	FMMOLootEntry Never;
	Never.Item = Sword;
	Never.DropChance = 0.0f;
	Table->Entries.Add(Never);

	UMMOLootContainerComponent* CorpseA = MakeContainer();
	UMMOLootContainerComponent* CorpseB = MakeContainer();
	CorpseA->GenerateFrom(Table);

	UMMOLootTable::ForceNextDrop(Sword);
	CorpseB->GenerateFrom(Table);

	TestEqual(TEXT("Corpse A: pelt only"), CorpseA->GetItems().Num(), 1);
	TestEqual(TEXT("Corpse B: pelt + forced sword"), CorpseB->GetItems().Num(), 2);
	TestEqual(TEXT("Forced drop consumed"), UMMOLootTable::GetPendingForcedDropCount(), 0);
	TestNotEqual(TEXT("Corpses have distinct stacks"), CorpseA->GetItems()[0].InstanceId, CorpseB->GetItems()[0].InstanceId);

	UMMOInventoryComponent* Inventory = MakeInventory(5);
	CorpseA->TakeAll(Inventory);
	TestFalse(TEXT("A emptied"), CorpseA->HasLoot());
	TestEqual(TEXT("B untouched by looting A"), CorpseB->GetItems().Num(), 2);

	// a new death replaces the old contents instead of adding to them
	CorpseB->GenerateFrom(Table);
	TestEqual(TEXT("Regenerated loot does not carry old stacks"), CorpseB->GetItems().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOEquipmentTest, "MMO.Items.Equipment.EquipUnequipSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOEquipmentTest::RunTest(const FString& Parameters)
{
	UMMOItemDefinition* TrainingSword = MakeItem(TEXT("TrainingSword"), 1, EMMOEquipmentSlot::MainHand, 10, 14);
	UMMOItemDefinition* Greyfang = MakeItem(TEXT("Greyfang"), 1, EMMOEquipmentSlot::MainHand, 17, 22);
	UMMOItemDefinition* Boots = MakeItem(TEXT("Boots"), 1, EMMOEquipmentSlot::Feet, 0, 0, 8);
	UMMOItemDefinition* Pelt = MakeItem(TEXT("Pelt"), 20);

	UMMOInventoryComponent* Inventory = MakeInventory(3);
	UMMOEquipmentComponent* Equipment = MakeEquipment();

	float Min = 0.0f, Max = 0.0f;
	Equipment->GetWeaponDamage(Min, Max);
	TestTrue(TEXT("Unarmed damage below the training sword"), Max < 10.0f);

	TestTrue(TEXT("Starting weapon equipped directly"), Equipment->EquipDirect(TrainingSword));
	Equipment->GetWeaponDamage(Min, Max);
	TestTrue(TEXT("Training sword damage 10-14"), Min == 10.0f && Max == 14.0f);

	// equip Greyfang from the bag: the training sword swaps into the same bag slot
	Inventory->AddItem(Greyfang, 1);
	Inventory->AddItem(Boots, 1);
	const int32 TotalBefore = Inventory->GetTotalItemCount() + Equipment->GetEquippedCount();
	TestEqual(TEXT("Equip Greyfang"), Equipment->EquipFromInventory(Inventory, 0), EMMOEquipResult::Success);
	TestEqual(TEXT("Greyfang worn"), Equipment->GetEquipped(EMMOEquipmentSlot::MainHand).Item.Get(), Greyfang);
	TestEqual(TEXT("Training sword swapped into the bag"), Inventory->GetSlot(0).Item.Get(), TrainingSword);
	Equipment->GetWeaponDamage(Min, Max);
	TestTrue(TEXT("Greyfang increases damage"), Min == 17.0f && Max == 22.0f);

	// boots apply armor
	TestEqual(TEXT("Equip boots"), Equipment->EquipFromInventory(Inventory, 1), EMMOEquipResult::Success);
	TestEqual(TEXT("Boots armor counted"), Equipment->GetTotalStats().Armor, 8.0f);
	TestTrue(TEXT("Boots slot emptied in bag"), Inventory->GetSlot(1).IsEmpty());

	// materials can't be equipped
	Inventory->AddItem(Pelt, 2);
	TestEqual(TEXT("Pelt not equippable"), Equipment->EquipFromInventory(Inventory, 1), EMMOEquipResult::NotEquippable);

	// repeated swaps never create or lose items
	for (int32 i = 0; i < 10; ++i)
	{
		Equipment->EquipFromInventory(Inventory, 0);
	}
	TestEqual(TEXT("Swapping 10 times conserves items"), Inventory->GetTotalItemCount() + Equipment->GetEquippedCount(), TotalBefore + 2);

	// unequip into a full bag fails without losing the item
	Inventory->AddItem(Pelt, 40);
	TestEqual(TEXT("Bag full"), Inventory->GetFreeSlotCount(), 0);
	TestEqual(TEXT("Unequip blocked when full"), Equipment->Unequip(EMMOEquipmentSlot::Feet, Inventory), EMMOEquipResult::InventoryFull);
	TestEqual(TEXT("Boots still worn"), Equipment->GetTotalStats().Armor, 8.0f);

	// unequip with room removes the stats
	Inventory->RemoveFromSlot(2, 20);
	TestEqual(TEXT("Unequip with room"), Equipment->Unequip(EMMOEquipmentSlot::Feet, Inventory), EMMOEquipResult::Success);
	TestEqual(TEXT("Armor removed"), Equipment->GetTotalStats().Armor, 0.0f);
	TestEqual(TEXT("Boots back in the bag"), Inventory->CountItem(Boots), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOArmorTest, "MMO.Items.Stats.ArmorReducesDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOArmorTest::RunTest(const FString& Parameters)
{
	UMMOHealthComponent* Health = NewObject<UMMOHealthComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	Health->MaxHealth = 100.0f;
	Health->ResetHealth();

	TestEqual(TEXT("No armor: full damage"), Health->ApplyDamage(10.0f, nullptr), 10.0f);

	Health->SetArmor(8.0f);
	const float Expected = 10.0f * (1.0f - 8.0f / 58.0f);
	TestTrue(TEXT("8 armor reduces damage by ~14%"), FMath::IsNearlyEqual(Health->ApplyDamage(10.0f, nullptr), Expected, 0.01f));

	Health->SetArmor(50.0f);
	TestTrue(TEXT("50 armor halves damage"), FMath::IsNearlyEqual(Health->GetDamageReduction(), 0.5f));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

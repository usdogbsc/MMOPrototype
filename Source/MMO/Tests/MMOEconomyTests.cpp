// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Combat/MMOCooldownComponent.h"
#include "Items/MMOActionBarComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Items/MMOVendor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace MMOEconomyTestUtils
{
	static UMMOItemDefinition* MakeTradeItem(FName Id, int32 MaxStack, int32 SellValue, int32 BuyPrice = 0)
	{
		UMMOItemDefinition* Item = NewObject<UMMOItemDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
		Item->ItemId = Id;
		Item->DisplayName = FText::FromName(Id);
		Item->MaxStackSize = MaxStack;
		Item->SellValue = SellValue;
		Item->BuyPrice = BuyPrice;
		return Item;
	}

	static UMMOInventoryComponent* MakeBag(int32 Capacity, int32 Money)
	{
		UMMOInventoryComponent* Inventory = NewObject<UMMOInventoryComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		Inventory->Capacity = Capacity;
		Inventory->EnsureSlots();
		Inventory->SetCurrency(Money);
		return Inventory;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOVendorBuySellTest, "MMO.Economy.VendorBuyAndSell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOVendorBuySellTest::RunTest(const FString& Parameters)
{
	using namespace MMOEconomyTestUtils;
	UMMOItemDefinition* Potion = MakeTradeItem(TEXT("ET_Potion"), 10, 6, 25);
	UMMOItemDefinition* Pelt = MakeTradeItem(TEXT("ET_Pelt"), 20, 3);
	UMMOItemDefinition* Junk = MakeTradeItem(TEXT("ET_Junk"), 1, 0);
	UMMOInventoryComponent* Bag = MakeBag(2, 60);

	TestEqual(TEXT("Buy price from the item"), Potion->GetBuyPrice(), 25);
	TestEqual(TEXT("Default buy price is 4x sell value"), Pelt->GetBuyPrice(), 12);

	TestEqual(TEXT("Buy two potions"), MMOVendor::Buy(Bag, Potion, 25, 2), EMMOVendorResult::Success);
	TestEqual(TEXT("Paid 50"), Bag->GetCurrency(), 10);
	TestEqual(TEXT("Got two"), Bag->CountItem(Potion), 2);
	TestEqual(TEXT("Not enough money"), MMOVendor::Buy(Bag, Potion, 25, 1), EMMOVendorResult::NotEnoughMoney);
	TestEqual(TEXT("Money unchanged after a failed buy"), Bag->GetCurrency(), 10);

	Bag->AddItem(Junk, 1);
	TestEqual(TEXT("Full bag refuses a new stack"), MMOVendor::Buy(Bag, Pelt, 3, 1), EMMOVendorResult::InventoryFull);
	TestEqual(TEXT("Money unchanged when the bag is full"), Bag->GetCurrency(), 10);
	TestEqual(TEXT("Full bag still tops up an existing stack"), MMOVendor::Buy(Bag, Potion, 5, 1), EMMOVendorResult::Success);

	FMMOItemStack Sold;
	int32 Copper = 0;
	const int32 JunkSlot = Bag->GetSlots().IndexOfByPredicate([Junk](const FMMOItemStack& S) { return S.Item == Junk; });
	TestEqual(TEXT("Worthless items can't be sold"), MMOVendor::Sell(Bag, JunkSlot, Sold, Copper), EMMOVendorResult::CannotSell);
	const int32 PotionSlot = Bag->GetSlots().IndexOfByPredicate([Potion](const FMMOItemStack& S) { return S.Item == Potion; });
	TestEqual(TEXT("Sell the potion stack"), MMOVendor::Sell(Bag, PotionSlot, Sold, Copper), EMMOVendorResult::Success);
	TestEqual(TEXT("Whole stack sold for sell value x quantity"), Copper, 18);
	TestTrue(TEXT("Sold stack returned for buyback"), Sold.Item == Potion && Sold.Quantity == 3);
	TestEqual(TEXT("Money received"), Bag->GetCurrency(), 5 + 18);
	TestEqual(TEXT("Potions gone"), Bag->CountItem(Potion), 0);
	TestEqual(TEXT("Selling an empty slot fails"), MMOVendor::Sell(Bag, PotionSlot, Sold, Copper), EMMOVendorResult::InvalidItem);

	TestFalse(TEXT("SpendCurrency refuses overdraft"), Bag->SpendCurrency(1000));
	TestTrue(TEXT("SpendCurrency"), Bag->SpendCurrency(3) && Bag->GetCurrency() == 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOActionBarTest, "MMO.Economy.ActionBar",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOActionBarTest::RunTest(const FString& Parameters)
{
	UMMOActionBarComponent* Bar = NewObject<UMMOActionBarComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	const FMMOActionSlot Potion = UMMOActionBarComponent::MakeItem(TEXT("Potion"));
	const FMMOActionSlot Bread = UMMOActionBarComponent::MakeItem(TEXT("Bread"));

	TestEqual(TEXT("Eight slots"), Bar->GetSlots().Num(), UMMOActionBarComponent::NumSlots);
	TestEqual(TEXT("Auto-place into the first slot"), Bar->AutoPlace(Potion), 0);
	TestEqual(TEXT("Auto-place doesn't duplicate"), Bar->AutoPlace(Potion), 0);
	TestEqual(TEXT("Next item goes to slot 2"), Bar->AutoPlace(Bread), 1);

	Bar->SetSlot(5, Potion);
	TestEqual(TEXT("Moving an action leaves its old slot"), Bar->FindSlot(Potion), 5);
	TestTrue(TEXT("Old slot empty"), Bar->GetSlot(0).IsEmpty());

	Bar->SwapSlots(1, 5);
	TestTrue(TEXT("Swap"), Bar->GetSlot(1) == Potion && Bar->GetSlot(5) == Bread);
	Bar->ClearSlot(1);
	TestEqual(TEXT("Cleared"), Bar->FindSlot(Potion), INDEX_NONE);

	for (int32 i = 0; i < UMMOActionBarComponent::NumSlots; ++i)
	{
		Bar->SetSlot(i, UMMOActionBarComponent::MakeItem(*FString::Printf(TEXT("Filler%d"), i)));
	}
	TestEqual(TEXT("Full bar can't auto-place"), Bar->AutoPlace(Potion), INDEX_NONE);
	TestEqual(TEXT("Out of range is empty"), Bar->GetSlot(99).Type, EMMOActionType::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMMOCooldownTest, "MMO.Economy.Cooldowns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMMOCooldownTest::RunTest(const FString& Parameters)
{
	double Clock = 100.0;
	UMMOCooldownComponent* Cooldowns = NewObject<UMMOCooldownComponent>(GetTransientPackage(), NAME_None, RF_Transient);
	Cooldowns->SetTimeSource([&Clock]() { return Clock; });

	TestTrue(TEXT("Ready by default"), Cooldowns->IsReady(TEXT("Potion")));
	Cooldowns->StartCooldown(TEXT("Potion"), 60.0f);
	TestFalse(TEXT("Not ready after use"), Cooldowns->IsReady(TEXT("Potion")));
	TestTrue(TEXT("Other groups unaffected"), Cooldowns->IsReady(TEXT("Food")));

	Clock += 45.0;
	TestEqual(TEXT("15s left"), Cooldowns->GetRemaining(TEXT("Potion")), 15.0f);
	TestEqual(TEXT("Duration kept for the sweep"), Cooldowns->GetDuration(TEXT("Potion")), 60.0f);

	Clock += 15.0;
	TestTrue(TEXT("Ready again"), Cooldowns->IsReady(TEXT("Potion")));
	TestEqual(TEXT("No duration once finished"), Cooldowns->GetDuration(TEXT("Potion")), 0.0f);

	Cooldowns->StartCooldown(TEXT("Food"), 0.0f);
	TestTrue(TEXT("Zero-length cooldown is ignored"), Cooldowns->IsReady(TEXT("Food")));
	return true;
}

#endif

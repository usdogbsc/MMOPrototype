// Copyright Epic Games, Inc. All Rights Reserved.

#include "Save/MMOSaveSubsystem.h"
#include "Save/MMOSaveGame.h"
#include "MMOCharacter.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Items/MMOEquipmentComponent.h"
#include "Items/MMOInventoryComponent.h"
#include "Items/MMOItemDefinition.h"
#include "Quests/MMOQuestDefinition.h"
#include "Quests/MMOQuestLogComponent.h"
#include "World/MMOExplorationComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "MMO.h"

const FString UMMOSaveSubsystem::DefaultSlot = TEXT("MMO_Character_0");

namespace MMOSave
{
	static FString GetMapName(const UWorld* World)
	{
		return World ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString();
	}
}

void UMMOSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	bPersistenceEnabled = !FParse::Param(FCommandLine::Get(), TEXT("MMONoSave"));
	SessionStart = FPlatformTime::Seconds();
	if (!bPersistenceEnabled)
	{
		UE_LOG(LogMMO, Display, TEXT("Saving disabled (-MMONoSave)"));
	}
}

double UMMOSaveSubsystem::GetPlayTime() const
{
	return LoadedPlayTime + (FPlatformTime::Seconds() - SessionStart);
}

UMMOSaveGame* UMMOSaveSubsystem::Capture(const AMMOCharacter* Character, UObject* Outer)
{
	if (!Character)
	{
		return nullptr;
	}

	UMMOSaveGame* Save = NewObject<UMMOSaveGame>(Outer ? Outer : GetTransientPackage());
	Save->SavedAt = FDateTime::Now();
	Save->Level = Character->GetProgression()->GetLevel();
	Save->XP = Character->GetProgression()->GetCurrentXP();

	// a dead character comes back like a respawn: full health at the start point
	const UMMOHealthComponent* Health = Character->GetHealth();
	Save->Health = Character->IsDead() ? Health->GetMaxHealth() : Health->GetCurrentHealth();
	Save->MapName = MMOSave::GetMapName(Character->GetWorld());
	Save->bHasLocation = !Character->IsDead();
	Save->Location = Character->GetActorLocation();
	Save->Yaw = Character->GetActorRotation().Yaw;

	const UMMOInventoryComponent* Inventory = Character->GetInventory();
	Save->Currency = Inventory->GetCurrency();
	const TArray<FMMOItemStack>& Slots = Inventory->GetSlots();
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].IsEmpty())
		{
			Save->Inventory.Add({ Slots[Index].Item->ItemId, Slots[Index].Quantity, Index });
		}
	}

	const UMMOEquipmentComponent* Equipment = Character->GetEquipment();
	for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EMMOEquipmentSlot::Count); ++SlotIndex)
	{
		const FMMOItemStack& Worn = Equipment->GetEquipped(static_cast<EMMOEquipmentSlot>(SlotIndex));
		if (!Worn.IsEmpty())
		{
			Save->Equipment.Add({ Worn.Item->ItemId, 1, SlotIndex });
		}
	}

	const UMMOQuestLogComponent* QuestLog = Character->GetQuestLog();
	for (const FMMOQuestProgress& Progress : QuestLog->GetActiveQuests())
	{
		if (Progress.Quest)
		{
			Save->ActiveQuests.Add({ Progress.Quest->QuestId, Progress.Counts });
		}
	}
	Save->CompletedQuests = QuestLog->GetCompletedQuestIds().Array();
	Save->Discovered = Character->GetExploration()->GetDiscovered().Array();
	return Save;
}

bool UMMOSaveSubsystem::Apply(const UMMOSaveGame* Save, AMMOCharacter* Character)
{
	if (!Save || !Character)
	{
		return false;
	}
	if (Save->Version > UMMOSaveGame::CurrentVersion)
	{
		UE_LOG(LogMMO, Warning, TEXT("Save game version %d is newer than this build understands (%d); not loading it"), Save->Version, UMMOSaveGame::CurrentVersion);
		return false;
	}

	Character->GetProgression()->RestoreProgress(Save->Level, Save->XP);

	// worn gear first (each item knows its slot), then the backpack exactly as it was laid out
	UMMOEquipmentComponent* Equipment = Character->GetEquipment();
	Equipment->ClearEquipment();
	for (const FMMOSavedItem& Saved : Save->Equipment)
	{
		if (!Equipment->EquipDirect(UMMOItemDefinition::FindById(Saved.ItemId)))
		{
			UE_LOG(LogMMO, Warning, TEXT("Save: could not re-equip '%s'"), *Saved.ItemId.ToString());
		}
	}

	UMMOInventoryComponent* Inventory = Character->GetInventory();
	Inventory->ClearInventory();
	for (const FMMOSavedItem& Saved : Save->Inventory)
	{
		UMMOItemDefinition* Item = UMMOItemDefinition::FindById(Saved.ItemId);
		if (!Item || !Inventory->AddStack(FMMOItemStack::Make(Item, Saved.Quantity), Saved.Slot))
		{
			UE_LOG(LogMMO, Warning, TEXT("Save: could not restore %d x '%s'"), Saved.Quantity, *Saved.ItemId.ToString());
		}
	}
	Inventory->SetCurrency(Save->Currency);

	Character->GetExploration()->RestoreDiscovered(TSet<FName>(Save->Discovered));

	TArray<FMMOQuestProgress> Active;
	for (const FMMOSavedQuest& Saved : Save->ActiveQuests)
	{
		if (UMMOQuestDefinition* Quest = UMMOQuestDefinition::FindById(Saved.QuestId))
		{
			FMMOQuestProgress& Progress = Active.AddDefaulted_GetRef();
			Progress.Quest = Quest;
			Progress.Counts = Saved.Counts;
		}
		else
		{
			UE_LOG(LogMMO, Warning, TEXT("Save: unknown quest '%s' dropped"), *Saved.QuestId.ToString());
		}
	}
	Character->GetQuestLog()->RestoreState(Active, TSet<FName>(Save->CompletedQuests));

	// position only applies to the map it was saved on
	if (Save->bHasLocation && Save->MapName == MMOSave::GetMapName(Character->GetWorld()))
	{
		Character->TeleportTo(Save->Location, FRotator(0.0f, Save->Yaw, 0.0f), false, true);
		if (AController* Controller = Character->GetController())
		{
			FRotator Control = Controller->GetControlRotation();
			Control.Yaw = Save->Yaw;
			Controller->SetControlRotation(Control);
		}
	}

	Character->RefreshAfterLoad(Save->Health);
	return true;
}

bool UMMOSaveSubsystem::SaveCharacter(const AMMOCharacter* Character, const FString& Slot)
{
	if (bSuppressSaves)
	{
		return false;
	}
	UMMOSaveGame* Save = Capture(Character, this);
	if (!Save)
	{
		return false;
	}
	Save->PlayTime = GetPlayTime();

	const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	UE_LOG(LogMMO, Log, TEXT("%s character to slot '%s' (level %d, %d items, %d quests)"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"),
		*Slot, Save->Level, Save->Inventory.Num() + Save->Equipment.Num(), Save->ActiveQuests.Num() + Save->CompletedQuests.Num());
	return bSaved;
}

bool UMMOSaveSubsystem::LoadCharacter(AMMOCharacter* Character, const FString& Slot)
{
	if (!HasSave(Slot))
	{
		return false;
	}

	const UMMOSaveGame* Save = Cast<UMMOSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (!Save)
	{
		UE_LOG(LogMMO, Warning, TEXT("Save slot '%s' could not be read"), *Slot);
		return false;
	}

	const bool bApplied = Apply(Save, Character);
	if (bApplied)
	{
		LoadedPlayTime = Save->PlayTime;
		SessionStart = FPlatformTime::Seconds();
		UE_LOG(LogMMO, Log, TEXT("Loaded character from slot '%s' (level %d, %d copper, %d items, %d places, at %s, saved %s)"), *Slot, Save->Level, Save->Currency, Save->Inventory.Num() + Save->Equipment.Num(), Save->Discovered.Num(), *Save->Location.ToCompactString(), *Save->SavedAt.ToString());
	}
	return bApplied;
}

bool UMMOSaveSubsystem::HasSave(const FString& Slot) const
{
	return UGameplayStatics::DoesSaveGameExist(Slot, 0);
}

bool UMMOSaveSubsystem::DeleteSave(const FString& Slot)
{
	return HasSave(Slot) && UGameplayStatics::DeleteGameInSlot(Slot, 0);
}

// ---------------------------------------------------------------------------------------------------------------------
// Console: mmo.save, mmo.load, mmo.newgame

namespace MMOSave
{
	static AMMOCharacter* GetPlayer(UWorld* World)
	{
		return World ? Cast<AMMOCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)) : nullptr;
	}

	static UMMOSaveSubsystem* GetSaves(UWorld* World)
	{
		return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UMMOSaveSubsystem>() : nullptr;
	}

	static void SaveCommand(const TArray<FString>& Args, UWorld* World)
	{
		AMMOCharacter* Player = GetPlayer(World);
		if (Player && Player->SaveNow())
		{
			Player->ShowPlayerMessage(NSLOCTEXT("MMOSave", "Saved", "Game saved."), false);
		}
		else
		{
			UE_LOG(LogMMO, Warning, TEXT("mmo.save: nothing saved (saving disabled, or no player)"));
		}
	}

	static void LoadCommand(const TArray<FString>& Args, UWorld* World)
	{
		UMMOSaveSubsystem* Saves = GetSaves(World);
		AMMOCharacter* Player = GetPlayer(World);
		if (!Saves || !Player || !Saves->LoadCharacter(Player))
		{
			UE_LOG(LogMMO, Warning, TEXT("mmo.load: no save to load"));
		}
	}

	static void NewGameCommand(const TArray<FString>& Args, UWorld* World)
	{
		UMMOSaveSubsystem* Saves = GetSaves(World);
		if (!Saves || !World)
		{
			return;
		}
		// stop this session from writing its state back as the level unloads
		Saves->SuppressSavesUntilNextLoad();
		Saves->DeleteSave();
		UE_LOG(LogMMO, Display, TEXT("mmo.newgame: save deleted, restarting the map"));
		UGameplayStatics::OpenLevel(World, FName(*GetMapName(World)));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMMOSaveCommand(TEXT("mmo.save"), TEXT("Saves the character now."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOSave::SaveCommand));
static FAutoConsoleCommandWithWorldAndArgs GMMOLoadCommand(TEXT("mmo.load"), TEXT("Reloads the character from the last save."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOSave::LoadCommand));
static FAutoConsoleCommandWithWorldAndArgs GMMONewGameCommand(TEXT("mmo.newgame"), TEXT("Deletes the saved character and restarts the map with a fresh one."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOSave::NewGameCommand));

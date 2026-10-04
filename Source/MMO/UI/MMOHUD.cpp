// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUD.h"
#include "UI/MMOHUDWidget.h"
#include "MMOCharacter.h"
#include "Items/MMOLootContainerComponent.h"
#include "NPC/MMONPC.h"
#include "World/MMOCraftingStation.h"
#include "UI/MMOMenuWidgets.h"
#include "Settings/MMOSettingsSubsystem.h"
#include "Save/MMOSaveSubsystem.h"
#include "Save/MMOSaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AMMOHUD::AMMOHUD()
{
	PrimaryActorTick.bCanEverTick = true;

	WindowOpenSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_WindowOpen.S_MMO_WindowOpen")));
	WindowCloseSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_WindowClose.S_MMO_WindowClose")));
}
#include "MMO.h"

void AMMOHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController())
	{
		return;
	}

	const TSubclassOf<UMMOHUDWidget> WidgetClass = HUDWidgetClass ? HUDWidgetClass : TSubclassOf<UMMOHUDWidget>(UMMOHUDWidget::StaticClass());
	HUDWidget = CreateWidget<UMMOHUDWidget>(PlayerOwner, WidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
		HUDWidget->OnWindowClosed.BindUObject(this, &AMMOHUD::HandleWindowClosed);
	}
	else
	{
		UE_LOG(LogMMO, Error, TEXT("Could not create the MMO HUD widget."));
	}

	LoadedWindowOpenSound = WindowOpenSound.LoadSynchronous();
	LoadedWindowCloseSound = WindowCloseSound.LoadSynchronous();

	UpdateInputMode();

	if (HUDWidget)
	{
		if (UMMOGameMenuWidget* Menu = HUDWidget->GetGameMenu())
		{
			Menu->OnResume.BindUObject(this, &AMMOHUD::ToggleGameMenu);
			Menu->OnSettings.BindUObject(this, &AMMOHUD::OpenSettings);
			Menu->OnControls.BindUObject(this, &AMMOHUD::OpenControls);
			Menu->OnQuitToTitle.BindUObject(this, &AMMOHUD::QuitToTitle);
			Menu->OnQuitGame.BindUObject(this, &AMMOHUD::QuitGame);
		}
		if (UMMOTitleScreenWidget* Title = HUDWidget->GetTitleScreen())
		{
			Title->OnContinue.BindUObject(this, &AMMOHUD::ContinueFromTitle);
			Title->OnNewAdventure.BindUObject(this, &AMMOHUD::StartNewAdventure);
			Title->OnSettings.BindUObject(this, &AMMOHUD::OpenSettings);
			Title->OnQuit.BindUObject(this, &AMMOHUD::QuitGame);
		}
	}

	// the title screen greets the player once per session
	UMMOSettingsSubsystem* Options = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMMOSettingsSubsystem>() : nullptr;
	if (HUDWidget && Options && Options->ShouldShowTitle())
	{
		Options->MarkTitleShown();
		const UMMOSaveSubsystem* Saves = GetGameInstance()->GetSubsystem<UMMOSaveSubsystem>();
		int32 Level = 1;
		const bool bHasSave = Saves && Saves->IsPersistenceEnabled() && Saves->HasSave();
		if (bHasSave)
		{
			if (const UMMOSaveGame* Save = Cast<UMMOSaveGame>(UGameplayStatics::LoadGameFromSlot(UMMOSaveSubsystem::DefaultSlot, 0)))
			{
				Level = Save->Level;
			}
		}
		HUDWidget->ShowTitle(bHasSave, Level);
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(HUDWidget->GetTitleScreen()->TakeWidget());
		PlayerOwner->SetInputMode(Mode);
		PlayerOwner->SetShowMouseCursor(true);
	}
}

void AMMOHUD::ContinueFromTitle()
{
	if (HUDWidget)
	{
		HUDWidget->HideTitle();
		HUDWidget->SetSettingsOpen(false);
	}
	if (PlayerOwner)
	{
		// back to classic MMO mouse controls
		PlayerOwner->SetShowMouseCursor(false);
		UpdateInputMode();
	}
	PlayUISound(LoadedWindowCloseSound);
}

void AMMOHUD::StartNewAdventure()
{
	UGameInstance* Instance = GetGameInstance();
	UMMOSaveSubsystem* Saves = Instance ? Instance->GetSubsystem<UMMOSaveSubsystem>() : nullptr;
	if (!Saves)
	{
		return;
	}
	// erase the old character and restart the map with a fresh one (straight into the game)
	Saves->SuppressSavesUntilNextLoad();
	Saves->DeleteSave();
	UGameplayStatics::OpenLevel(this, FName(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName())));
}

void AMMOHUD::QuitToTitle()
{
	if (AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPawn()))
	{
		Character->SaveNow();
	}
	if (UMMOSettingsSubsystem* Options = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMMOSettingsSubsystem>() : nullptr)
	{
		Options->RequestTitle();
	}
	UGameplayStatics::OpenLevel(this, FName(*UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName())));
}

void AMMOHUD::QuitGame()
{
	if (AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPawn()))
	{
		Character->SaveNow();
	}
	UKismetSystemLibrary::QuitGame(this, PlayerOwner, EQuitPreference::Quit, false);
}

void AMMOHUD::ToggleGameMenu()
{
	if (HUDWidget && !HUDWidget->IsTitleOpen())
	{
		const bool bOpen = !HUDWidget->IsGameMenuOpen();
		HUDWidget->SetGameMenuOpen(bOpen);
		PlayUISound(bOpen ? LoadedWindowOpenSound : LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::OpenSettings()
{
	if (HUDWidget)
	{
		HUDWidget->SetGameMenuOpen(false);
		HUDWidget->SetSettingsOpen(true);
		PlayUISound(LoadedWindowOpenSound);
	}
}

void AMMOHUD::OpenControls()
{
	if (HUDWidget)
	{
		HUDWidget->SetGameMenuOpen(false);
		HUDWidget->SetControlsOpen(true);
		PlayUISound(LoadedWindowOpenSound);
	}
}

void AMMOHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// keep the widget bound to the current pawn even when it isn't being painted
	if (HUDWidget)
	{
		HUDWidget->SyncToOwningPawn();
	}

	// the loot window closes when its corpse is emptied, despawns, or the player walks away
	if (HUDWidget && HUDWidget->IsLootOpen())
	{
		const AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPawn());
		const UMMOLootContainerComponent* Container = HUDWidget->GetOpenLoot();
		if (!Character || !Container || !Container->HasLoot() || !Character->CanReachLoot(Container))
		{
			CloseLoot();
		}
	}

	// crafting needs the station in reach
	if (HUDWidget && HUDWidget->IsCraftingOpen())
	{
		const AMMOCraftingStation* Station = HUDWidget->GetOpenStation();
		if (!Station || !Station->IsInReach(GetOwningPawn()))
		{
			CloseCrafting();
		}
	}

	// so does trading
	if (HUDWidget && HUDWidget->IsVendorOpen())
	{
		const AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPawn());
		if (!Character || !Character->CanTradeWith(HUDWidget->GetOpenVendor()))
		{
			CloseVendor();
		}
	}

	// conversations end when the player walks away from the NPC
	if (HUDWidget && HUDWidget->IsDialogueOpen())
	{
		const APawn* Pawn = GetOwningPawn();
		const AMMONPC* NPC = HUDWidget->GetDialogueNPC();
		if (!Pawn || !NPC || FVector::Dist2D(Pawn->GetActorLocation(), NPC->GetActorLocation()) > NPC->GetInteractRange() + 250.0f)
		{
			CloseDialogue();
		}
	}
}

void AMMOHUD::ToggleQuestLog()
{
	if (HUDWidget)
	{
		const bool bOpen = !HUDWidget->IsQuestLogOpen();
		HUDWidget->SetQuestLogOpen(bOpen);
		PlayUISound(bOpen ? LoadedWindowOpenSound : LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::ToggleAbilities()
{
	if (HUDWidget)
	{
		const bool bOpen = !HUDWidget->IsAbilitiesOpen();
		if (AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPawn()); Character && bOpen)
		{
			Character->MarkTutorial(TEXT("Abilities"));
		}
		HUDWidget->SetAbilitiesOpen(bOpen);
		PlayUISound(bOpen ? LoadedWindowOpenSound : LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::OpenCrafting(AMMOCraftingStation* Station)
{
	if (HUDWidget && Station)
	{
		const bool bWasOpen = HUDWidget->IsCraftingOpen();
		HUDWidget->CloseDialogue();
		HUDWidget->CloseVendor();
		HUDWidget->CloseLoot();
		HUDWidget->OpenCrafting(Station);
		if (!bWasOpen)
		{
			PlayUISound(LoadedWindowOpenSound);
		}
		UpdateInputMode();
	}
}

void AMMOHUD::CloseCrafting()
{
	if (HUDWidget && HUDWidget->IsCraftingOpen())
	{
		HUDWidget->CloseCrafting();
		PlayUISound(LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::OpenDialogue(AMMONPC* NPC)
{
	if (HUDWidget && NPC)
	{
		const bool bWasOpen = HUDWidget->IsDialogueOpen();
		HUDWidget->CloseLoot();
		HUDWidget->OpenDialogue(NPC);
		if (!bWasOpen)
		{
			PlayUISound(LoadedWindowOpenSound);
		}
		UpdateInputMode();
	}
}

void AMMOHUD::OpenVendor(AMMONPC* Vendor)
{
	if (HUDWidget && Vendor)
	{
		const bool bWasOpen = HUDWidget->IsVendorOpen();
		HUDWidget->CloseDialogue();
		HUDWidget->CloseLoot();
		HUDWidget->OpenVendor(Vendor);
		if (!bWasOpen)
		{
			PlayUISound(LoadedWindowOpenSound);
		}
		UpdateInputMode();
	}
}

void AMMOHUD::CloseVendor()
{
	if (HUDWidget && HUDWidget->IsVendorOpen())
	{
		HUDWidget->CloseVendor();
		PlayUISound(LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::CloseDialogue()
{
	if (HUDWidget && HUDWidget->IsDialogueOpen())
	{
		HUDWidget->CloseDialogue();
		PlayUISound(LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::ToggleInventory()
{
	if (HUDWidget)
	{
		const bool bOpen = !HUDWidget->IsInventoryOpen();
		if (AMMOCharacter* Character = Cast<AMMOCharacter>(GetOwningPawn()); Character && bOpen)
		{
			Character->MarkTutorial(TEXT("Backpack"));
		}
		HUDWidget->SetInventoryOpen(bOpen);
		PlayUISound(bOpen ? LoadedWindowOpenSound : LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::ToggleCharacter()
{
	if (HUDWidget)
	{
		const bool bOpen = !HUDWidget->IsCharacterOpen();
		HUDWidget->SetCharacterOpen(bOpen);
		PlayUISound(bOpen ? LoadedWindowOpenSound : LoadedWindowCloseSound);
		UpdateInputMode();
	}
}

void AMMOHUD::OpenLoot(UMMOLootContainerComponent* Container)
{
	if (HUDWidget && Container)
	{
		const bool bWasOpen = HUDWidget->IsLootOpen();
		HUDWidget->OpenLoot(Container);
		if (!bWasOpen)
		{
			PlayUISound(LoadedWindowOpenSound);
		}
		UpdateInputMode();
	}
}

void AMMOHUD::CloseLoot()
{
	if (HUDWidget && HUDWidget->IsLootOpen())
	{
		HUDWidget->CloseLoot();
		UpdateInputMode();
	}
}

bool AMMOHUD::CloseAllWindows()
{
	if (!IsAnyWindowOpen())
	{
		return false;
	}

	HUDWidget->SetInventoryOpen(false);
	HUDWidget->SetCharacterOpen(false);
	HUDWidget->CloseLoot();
	HUDWidget->CloseDialogue();
	HUDWidget->CloseVendor();
	HUDWidget->SetQuestLogOpen(false);
	HUDWidget->SetAbilitiesOpen(false);
	HUDWidget->CloseCrafting();
	HUDWidget->SetGameMenuOpen(false);
	HUDWidget->SetSettingsOpen(false);
	HUDWidget->SetControlsOpen(false);
	PlayUISound(LoadedWindowCloseSound);
	UpdateInputMode();
	return true;
}

bool AMMOHUD::IsAnyWindowOpen() const
{
	return HUDWidget && (HUDWidget->IsInventoryOpen() || HUDWidget->IsCharacterOpen() || HUDWidget->IsLootOpen() || HUDWidget->IsDialogueOpen() || HUDWidget->IsQuestLogOpen() || HUDWidget->IsVendorOpen() || HUDWidget->IsAbilitiesOpen() || HUDWidget->IsCraftingOpen()
		|| HUDWidget->IsGameMenuOpen() || HUDWidget->IsSettingsOpen() || HUDWidget->IsControlsOpen());
}

void AMMOHUD::HandleWindowClosed()
{
	UpdateInputMode();
}

void AMMOHUD::UpdateInputMode()
{
	if (!PlayerOwner)
	{
		return;
	}

	// classic MMO mouse: the cursor is always free; holding a mouse button over the world turns the camera
	// (the viewport captures the mouse only while a button is down, and hides the cursor during the drag)
	if (!PlayerOwner->bShowMouseCursor)
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(true);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerOwner->SetInputMode(Mode);
		PlayerOwner->SetShowMouseCursor(true);
	}
}

void AMMOHUD::PlayUISound(USoundBase* Sound) const
{
	if (Sound)
	{
		UGameplayStatics::PlaySound2D(this, Sound);
	}
}

void AMMOHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

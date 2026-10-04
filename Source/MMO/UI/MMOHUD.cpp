// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUD.h"
#include "UI/MMOHUDWidget.h"
#include "MMOCharacter.h"
#include "Items/MMOLootContainerComponent.h"
#include "NPC/MMONPC.h"
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
	PlayUISound(LoadedWindowCloseSound);
	UpdateInputMode();
	return true;
}

bool AMMOHUD::IsAnyWindowOpen() const
{
	return HUDWidget && (HUDWidget->IsInventoryOpen() || HUDWidget->IsCharacterOpen() || HUDWidget->IsLootOpen() || HUDWidget->IsDialogueOpen() || HUDWidget->IsQuestLogOpen() || HUDWidget->IsVendorOpen());
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

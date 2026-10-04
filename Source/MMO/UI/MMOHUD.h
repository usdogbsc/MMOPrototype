// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MMOHUD.generated.h"

class UMMOHUDWidget;
class UMMOLootContainerComponent;
class AMMONPC;
class AMMOCraftingStation;
class USoundBase;

/**
 *  Owns the UMG MMO HUD for the local player.
 *  Set HUDWidgetClass to a Widget Blueprint child of UMMOHUDWidget to replace the default C++ layout.
 */
UCLASS()
class AMMOHUD : public AHUD
{
	GENERATED_BODY()

public:

	UMMOHUDWidget* GetHUDWidget() const { return HUDWidget; }

	void ToggleInventory();
	void ToggleCharacter();
	void OpenLoot(UMMOLootContainerComponent* Container);
	void CloseLoot();
	void ToggleQuestLog();
	void ToggleAbilities();
	void ToggleGameMenu();
	void OpenSettings();
	void OpenControls();

	/** Title screen buttons */
	void ContinueFromTitle();
	void StartNewAdventure();
	void QuitToTitle();
	void QuitGame();
	void OpenCrafting(AMMOCraftingStation* Station);
	void CloseCrafting();
	void OpenDialogue(AMMONPC* NPC);
	void OpenVendor(AMMONPC* Vendor);
	void CloseVendor();
	void CloseDialogue();

	/** Closes every open window. Returns true if anything was open */
	bool CloseAllWindows();

	bool IsAnyWindowOpen() const;

protected:

	/** HUD widget class to create. Defaults to the C++ layout */
	UPROPERTY(EditAnywhere, Category="HUD")
	TSubclassOf<UMMOHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMMOHUDWidget> HUDWidget;

	UPROPERTY(EditAnywhere, Category="HUD|Audio")
	TSoftObjectPtr<USoundBase> WindowOpenSound;

	UPROPERTY(EditAnywhere, Category="HUD|Audio")
	TSoftObjectPtr<USoundBase> WindowCloseSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedWindowOpenSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedWindowCloseSound;

	/** Keeps the mouse cursor visible and free (MMO-style controls) */
	void UpdateInputMode();

	void PlayUISound(USoundBase* Sound) const;

	void HandleWindowClosed();

public:

	AMMOHUD();

protected:

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};

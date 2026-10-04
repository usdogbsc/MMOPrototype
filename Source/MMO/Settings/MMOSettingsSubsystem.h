// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MMOSettingsSubsystem.generated.h"

/** Player options, stored in Saved/SaveGames/MMO_Settings.sav */
UCLASS()
class UMMOSettingsSave : public USaveGame
{
	GENERATED_BODY()

public:

	UPROPERTY()
	float MasterVolume = 1.0f;

	/** Camera turn speed multiplier */
	UPROPERTY()
	float MouseSensitivity = 1.0f;

	UPROPERTY()
	bool bInvertY = false;

	/** The one-line control reminder under the unit frames */
	UPROPERTY()
	bool bShowKeyHints = true;

	/** First-session guide tips */
	UPROPERTY()
	bool bShowTutorial = true;

	/** 0 Low, 1 Medium, 2 High, 3 Epic */
	UPROPERTY()
	int32 GraphicsQuality = 3;

	UPROPERTY()
	bool bFullscreen = false;

	UPROPERTY()
	bool bVSync = true;

	/** 0 = unlimited */
	UPROPERTY()
	int32 FrameRateLimit = 0;
};

/**
 *  Loads, applies and saves player options. Audio and controls apply immediately; graphics are only
 *  touched once the player has changed them (so automated runs keep the engine defaults).
 */
UCLASS()
class UMMOSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	static const FString SlotName;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UMMOSettingsSave* Get() const { return Settings; }

	/** Applies everything and writes the settings file */
	void ApplyAndSave();

	void ApplyAudio() const;
	void ApplyGraphics() const;

	/** Title screen once per session (not in automated runs or with -MMONoTitle) */
	bool ShouldShowTitle() const;
	void MarkTitleShown() { bTitleShown = true; }
	void RequestTitle() { bTitleShown = false; }

	/** Quality names for the UI */
	static FText GetQualityName(int32 Quality);

protected:

	UPROPERTY()
	TObjectPtr<UMMOSettingsSave> Settings;

	/** True once settings were saved by the player at least once */
	bool bHasSavedSettings = false;

	bool bTitleShown = false;
};

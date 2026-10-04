// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMOMenuWidgets.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UMMOTextButtonWidget;
class UMMOSettingsSubsystem;

/** Full-screen title: game name, Continue / New Adventure / Settings / Quit */
UCLASS()
class UMMOTitleScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Rebuilds the buttons (Continue only when a saved character exists) */
	void Setup(bool bHasSave, int32 SavedLevel);

	FSimpleDelegate OnContinue;
	FSimpleDelegate OnNewAdventure;
	FSimpleDelegate OnSettings;
	FSimpleDelegate OnQuit;

protected:

	UPROPERTY()
	TObjectPtr<UVerticalBox> Buttons;

	UPROPERTY()
	TObjectPtr<UTextBlock> FooterText;

	bool bConfirmNewAdventure = false;
	bool bHasSavedCharacter = false;
	int32 Level = 1;

	virtual void NativeOnInitialized() override;
	void Rebuild();
};

/** Esc menu: Resume, Settings, Controls, Quit to Title, Quit Game */
UCLASS()
class UMMOGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	FSimpleDelegate OnResume;
	FSimpleDelegate OnSettings;
	FSimpleDelegate OnControls;
	FSimpleDelegate OnQuitToTitle;
	FSimpleDelegate OnQuitGame;

protected:

	virtual void NativeOnInitialized() override;
};

/** Options: each row is "label   < value >" */
UCLASS()
class UMMOSettingsWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(UMMOSettingsSubsystem* InSettings);
	void Refresh();

	FSimpleDelegate OnCloseRequested;

protected:

	UPROPERTY()
	TObjectPtr<UVerticalBox> Rows;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<UMMOSettingsSubsystem> Settings;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClose();

	void AddRow(const FString& Label, const FString& Value, TFunction<void(int32)> OnStep);
};

/** Read-only list of controls */
UCLASS()
class UMMOControlsWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	FSimpleDelegate OnCloseRequested;

protected:

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClose();
};

/** First-session guide: one tip at a time, with a button to turn tips off */
UCLASS()
class UMMOGuideWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetTip(const FText& Tip);

	FSimpleDelegate OnHideTips;

protected:

	UPROPERTY()
	TObjectPtr<UTextBlock> TipText;

	virtual void NativeOnInitialized() override;
};

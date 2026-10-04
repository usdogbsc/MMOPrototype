// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMOQuestWidgets.generated.h"

class AMMOCharacter;
class AMMONPC;
class UButton;
class UTextBlock;
class UVerticalBox;
class UWidgetTree;
class UMMOQuestDefinition;
class UMMOQuestLogComponent;

/** Shared quest presentation used by dialogue, quest log and tracker */
namespace MMOQuestUI
{
	/** Adds objectives (with progress if the quest is accepted) to Box */
	void AddObjectives(UWidgetTree* Tree, UVerticalBox* Box, const UMMOQuestDefinition* Quest, const UMMOQuestLogComponent* Log);

	/** Adds the reward block (XP, currency, items) to Box */
	void AddRewards(UWidgetTree* Tree, UVerticalBox* Box, const UMMOQuestDefinition* Quest);

	UTextBlock* AddParagraph(UWidgetTree* Tree, UVerticalBox* Box, const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = false, float TopPadding = 0.0f);
}

/**
 *  NPC conversation window: greeting and quest list, then quest pages (offer / progress / complete)
 *  with Accept, Complete and Back buttons.
 */
UCLASS()
class UMMODialogueWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Open(AMMOCharacter* InCharacter, AMMONPC* InNPC);
	void Close();

	AMMONPC* GetNPC() const { return NPC.Get(); }

	/** The quest page currently shown (null on the greeting page) */
	UMMOQuestDefinition* GetShownQuest() const { return ShownQuest.Get(); }

	/** Greeting page: lists the NPC's relevant quests */
	void ShowGreeting();

	/** Quest page: offer, progress or completion depending on state */
	void ShowQuest(UMMOQuestDefinition* Quest);

	FSimpleDelegate OnCloseRequested;

protected:

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Body;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Buttons;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;
	TWeakObjectPtr<AMMONPC> NPC;
	TWeakObjectPtr<UMMOQuestDefinition> ShownQuest;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClose();

	void AddButton(const FString& Label, const FLinearColor& Color, TFunction<void()> OnClick, bool bLeftAligned = false);
	UMMOQuestLogComponent* GetLog() const;
};

/** Quest log (L): list of accepted quests, details and Abandon */
UCLASS()
class UMMOQuestLogWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(AMMOCharacter* InCharacter);

	UFUNCTION()
	void Refresh();

	FSimpleDelegate OnCloseRequested;

protected:

	UPROPERTY()
	TObjectPtr<UVerticalBox> List;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Details;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;
	TWeakObjectPtr<UMMOQuestDefinition> Selected;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClose();
};

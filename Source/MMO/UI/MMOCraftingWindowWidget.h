// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Professions/MMOProfessionTypes.h"
#include "MMOCraftingWindowWidget.generated.h"

class AMMOCharacter;
class AMMOCraftingStation;
class UButton;
class UTextBlock;
class UVerticalBox;

/**
 *  Crafting window for one station: the profession's skill, each recipe colored by difficulty with its
 *  reagents (have / need), and Craft / Craft All buttons.
 */
UCLASS()
class UMMOCraftingWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Open(AMMOCharacter* InCharacter, AMMOCraftingStation* InStation);
	void Close();

	AMMOCraftingStation* GetStation() const { return Station.Get(); }

	UFUNCTION()
	void Refresh();

	FSimpleDelegate OnCloseRequested;

protected:

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SkillText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> List;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;
	TWeakObjectPtr<AMMOCraftingStation> Station;
	FDelegateHandle ActivityHandle;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClose();

	UFUNCTION()
	void HandleSkillChanged(EMMOProfession Profession, int32 NewSkill);

	void Unbind();
};

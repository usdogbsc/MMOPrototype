// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/MMOItemTypes.h"
#include "MMOCharacterWindowWidget.generated.h"

class AMMOCharacter;
class UButton;
class UTextBlock;
class UVerticalBox;
class UMMOItemSlotWidget;

/**
 *  Character window: name, level, core combat stats and the equipment slots.
 *  Right-click / double-click a worn item to unequip; drag a bag item onto its slot to equip.
 */
UCLASS()
class UMMOCharacterWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(AMMOCharacter* InCharacter);

	UFUNCTION()
	void Refresh();

	FSimpleDelegate OnCloseRequested;

	UMMOItemSlotWidget* GetSlotWidget(EMMOEquipmentSlot EquipSlot) const;

protected:

	UPROPERTY()
	TMap<EMMOEquipmentSlot, TObjectPtr<UMMOItemSlotWidget>> SlotWidgets;

	UPROPERTY()
	TObjectPtr<UTextBlock> HeaderText;

	UPROPERTY()
	TObjectPtr<UTextBlock> HealthStatText;

	UPROPERTY()
	TObjectPtr<UTextBlock> DamageStatText;

	UPROPERTY()
	TObjectPtr<UTextBlock> ArmorStatText;

	UPROPERTY()
	TObjectPtr<UTextBlock> SwingStatText;

	/** One row per profession */
	UPROPERTY()
	TArray<TObjectPtr<UTextBlock>> ProfessionTexts;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildLayout();
	UWidget* MakeSlotRow(EMMOEquipmentSlot EquipSlot, bool bLabelOnLeft);
	UTextBlock* AddStatRow(UVerticalBox* Box, const FString& Label);
	void RefreshStats();

	UFUNCTION()
	void HandleClose();

	void HandleSlotUse(UMMOItemSlotWidget* SlotWidget);
	void HandleSlotDropped(UMMOItemSlotWidget* Target, UMMOItemSlotWidget* Source);
};

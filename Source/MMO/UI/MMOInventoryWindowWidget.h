// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMOInventoryWindowWidget.generated.h"

class AMMOCharacter;
class UButton;
class UTextBlock;
class UUniformGridPanel;
class UMMOItemSlotWidget;

/**
 *  Backpack window: slot grid, currency, tooltips.
 *  Right-click / double-click equips; drag moves or stacks items, or drops worn gear back into the bag.
 */
UCLASS()
class UMMOInventoryWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(AMMOCharacter* InCharacter);

	UFUNCTION()
	void Refresh();

	FSimpleDelegate OnCloseRequested;

	/** Slot widgets, in inventory order (for tests) */
	const TArray<TObjectPtr<UMMOItemSlotWidget>>& GetSlotWidgets() const { return SlotWidgets; }

protected:

	UPROPERTY()
	TArray<TObjectPtr<UMMOItemSlotWidget>> SlotWidgets;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> Grid;

	UPROPERTY()
	TObjectPtr<UTextBlock> CurrencyText;

	UPROPERTY()
	TObjectPtr<UTextBlock> FreeSlotsText;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;

	/** Slots per row */
	int32 Columns = 5;

	virtual void NativeOnInitialized() override;

	void BuildLayout();
	void RebuildSlots(int32 Count);

	UFUNCTION()
	void HandleClose();

	void HandleSlotUse(UMMOItemSlotWidget* SlotWidget);
	void HandleSlotDropped(UMMOItemSlotWidget* Target, UMMOItemSlotWidget* Source);
};

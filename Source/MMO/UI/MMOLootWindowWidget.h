// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Items/MMOItemTypes.h"
#include "MMOLootWindowWidget.generated.h"

class AMMOCharacter;
class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UMMOItemSlotWidget;
class UMMOLootContainerComponent;

DECLARE_DELEGATE_OneParam(FMMOLootRowClicked, class UMMOLootRowWidget* /*Row*/);

/** One clickable line in the loot window */
UCLASS()
class UMMOLootRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetItem(const FMMOItemStack& Stack);
	void SetCurrency(int32 Amount);

	bool IsCurrency() const { return bCurrency; }
	const FGuid& GetInstanceId() const { return InstanceId; }

	FMMOLootRowClicked OnClicked;

protected:

	UPROPERTY()
	TObjectPtr<UBorder> Background;

	UPROPERTY()
	TObjectPtr<UMMOItemSlotWidget> ItemSlot;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> QuantityText;

	FGuid InstanceId;
	bool bCurrency = false;

	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
};

/**
 *  Loot window for one corpse: click a row to take it, or Loot All.
 *  The HUD closes it when the corpse is emptied, despawns or the player walks away.
 */
UCLASS()
class UMMOLootWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Open(AMMOCharacter* InCharacter, UMMOLootContainerComponent* InContainer);
	void Close();

	UMMOLootContainerComponent* GetContainer() const { return Container.Get(); }

	UFUNCTION()
	void Refresh();

	UFUNCTION()
	void HandleLootAll();

	FSimpleDelegate OnCloseRequested;

	int32 GetRowCount() const;

protected:

	UPROPERTY()
	TObjectPtr<UVerticalBox> Rows;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UButton> LootAllButton;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;
	TWeakObjectPtr<UMMOLootContainerComponent> Container;

	virtual void NativeOnInitialized() override;

	void BuildLayout();
	void HandleRowClicked(UMMOLootRowWidget* Row);

	UFUNCTION()
	void HandleClose();
};

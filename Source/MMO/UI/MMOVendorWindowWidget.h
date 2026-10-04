// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMOVendorWindowWidget.generated.h"

class AMMOCharacter;
class AMMONPC;
class UButton;
class UTextBlock;
class UVerticalBox;

/** Merchant window: stock with prices and Buy buttons, recent sales to buy back, and the player's money */
UCLASS()
class UMMOVendorWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Open(AMMOCharacter* InCharacter, AMMONPC* InVendor);
	void Close();

	AMMONPC* GetVendor() const { return Vendor.Get(); }

	UFUNCTION()
	void Refresh();

	FSimpleDelegate OnCloseRequested;

protected:

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY()
	TObjectPtr<UVerticalBox> StockList;

	UPROPERTY()
	TObjectPtr<UVerticalBox> BuybackList;

	UPROPERTY()
	TObjectPtr<UTextBlock> CurrencyText;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;
	TWeakObjectPtr<AMMONPC> Vendor;
	FDelegateHandle TradeHandle;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClose();

	void Unbind();
};

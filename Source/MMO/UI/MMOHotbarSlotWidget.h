// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMOHotbarSlotWidget.generated.h"

class UBorder;
class UTextBlock;
class UProgressBar;

/**
 *  Single hotbar slot for Basic Attack / Auto Attack.
 *  Shows the key, the swing timer sweep, an active (pulsing gold) state while auto-attacking and a red tint when out of range.
 */
UCLASS()
class UMMOHotbarSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetSlotState(bool bAutoAttackActive, bool bOutOfRange, float SwingRemaining, float SwingInterval);

protected:

	UPROPERTY(BlueprintReadOnly, Category="Hotbar", meta=(BindWidgetOptional))
	TObjectPtr<UBorder> SlotBackground;

	/** Outline shown while auto-attack is active */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar", meta=(BindWidgetOptional))
	TObjectPtr<UBorder> ActiveFrame;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> KeyText;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LabelText;

	/** Dark sweep showing the swing timer */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> SwingTimerBar;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StateText;

	bool bActive = false;
	float PulseTime = 0.0f;

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildDefaultLayout();
};

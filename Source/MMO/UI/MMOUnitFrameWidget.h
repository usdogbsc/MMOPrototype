// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMOUnitFrameWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UWidget;

/**
 *  Unit frame used for both the player and the target: name, level, health (with a trailing "chip" bar) and optional XP.
 *  Builds a default layout in C++. A Widget Blueprint child can provide its own layout by naming widgets to match the bindings.
 */
UCLASS()
class UMMOUnitFrameWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetUnitName(const FText& Name, const FLinearColor& Color);
	void SetLevel(int32 Level);
	void SetHealth(float Current, float Max, bool bDead = false);
	void SetXP(int32 Current, int32 Required, bool bMaxLevel);
	void SetShowXP(bool bShow);

	/** Snaps the trailing health bar (use when switching targets) */
	void ResetHealthChip();

protected:

	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	/** Light bar behind the health bar that drains after damage so hits are easy to read */
	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthChipBar;

	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> XPRow;

	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> XPBar;

	UPROPERTY(BlueprintReadOnly, Category="Unit Frame", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> XPText;

	/** Health bar fill color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Unit Frame")
	FLinearColor HealthColor = FLinearColor(0.16f, 0.72f, 0.22f);

	float HealthPercent = 1.0f;
	float ChipPercent = 1.0f;
	float ChipHoldTime = 0.0f;

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildDefaultLayout();

public:

	void SetHealthColor(const FLinearColor& Color);
};

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MMONameplateWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UWidget;

/**
 *  Overhead creature nameplate (used by AMMOCreature's screen-space widget component).
 *  Name + small health bar; gold and bracketed while targeted, grey when dead.
 */
UCLASS()
class UMMONameplateWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetNameplateState(const FText& Name, int32 Level, float HealthPercent, bool bTargeted, bool bDead, bool bInCombat, bool bLootable = false);

protected:

	UPROPERTY(BlueprintReadOnly, Category="Nameplate", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(BlueprintReadOnly, Category="Nameplate", meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(BlueprintReadOnly, Category="Nameplate", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> HealthRow;

	FString CachedLabel;

	virtual void NativeOnInitialized() override;

	void BuildDefaultLayout();
};

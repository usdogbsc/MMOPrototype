// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Combat/MMOHealthComponent.h"
#include "MMOHUDWidget.generated.h"

class AMMOCharacter;
class UMMOUnitFrameWidget;
class UMMOHotbarSlotWidget;
class UCanvasPanel;
class UTextBlock;
class UImage;
class UWidget;

/**
 *  Root MMO HUD: player frame, target frame, Basic Attack hotbar slot, floating combat text, banners and screen flashes.
 *  Builds a default layout in C++; a Widget Blueprint child can supply its own layout using the same widget names.
 */
UCLASS()
class UMMOHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	/** Spawns a floating combat text at a world location */
	void AddFloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color, int32 FontSize = 24, float Duration = 1.2f);

	/** True once the player and target frames have been built/bound */
	bool HasFrames() const { return PlayerFrame && TargetFrame; }

	UMMOUnitFrameWidget* GetTargetFrame() const { return TargetFrame; }

protected:

	/** Unit frame class used by the default layout */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	TSubclassOf<UMMOUnitFrameWidget> UnitFrameClass;

	/** Hotbar slot class used by the default layout */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	TSubclassOf<UMMOHotbarSlotWidget> HotbarSlotClass;

	/** Name shown in the player frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FText PlayerDisplayName = NSLOCTEXT("MMOHUD", "PlayerName", "Adventurer");

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOUnitFrameWidget> PlayerFrame;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOUnitFrameWidget> TargetFrame;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UMMOHotbarSlotWidget> AttackSlot;

	/** Full-screen canvas that floating combat text is placed on */
	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UCanvasPanel> FloatingTextLayer;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ErrorText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> LevelUpBanner;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelUpSubtitle;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UWidget> DeathOverlay;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> DeathCountdownText;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> DamageFlash;

	UPROPERTY(BlueprintReadOnly, Category="HUD", meta=(BindWidgetOptional))
	TObjectPtr<UImage> LevelUpFlash;

	struct FFloatingText
	{
		TWeakObjectPtr<UTextBlock> Widget;
		FVector WorldLocation;
		float Age = 0.0f;
		float Duration = 1.2f;
		float JitterX = 0.0f;
	};

	TArray<FFloatingText> FloatingTexts;

	float ErrorTime = 0.0f;
	float LevelUpTime = 0.0f;
	float DamageFlashTime = 0.0f;

	TWeakObjectPtr<AMMOCharacter> BoundCharacter;
	TWeakObjectPtr<AActor> LastTarget;
	FDelegateHandle CombatEventHandle;

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void BuildDefaultLayout();

	void BindToCharacter(AMMOCharacter* Character);
	void UnbindFromCharacter();

	void UpdateFrames(AMMOCharacter* Character);
	void UpdateFloatingTexts(float DeltaSeconds);
	void UpdateBanners(AMMOCharacter* Character, float DeltaSeconds);

	void HandleAnyCombatEvent(const UMMOHealthComponent* Component, EMMOCombatEvent Event, float Amount);

	UFUNCTION()
	void HandleCombatError(const FText& Message);

	UFUNCTION()
	void HandleLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleXPChanged(int32 CurrentXP, int32 XPToNextLevel, int32 XPGained);

	UFUNCTION()
	void HandlePlayerDamaged(float Amount, AActor* DamageInstigator);
};

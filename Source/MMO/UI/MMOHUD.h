// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Combat/MMOHealthComponent.h"
#include "MMOHUD.generated.h"

class AMMOCharacter;
class UFont;

/**
 *  Minimal MMO HUD drawn with the Canvas so it needs no widget assets:
 *  player frame, target frame, XP bar, Basic Attack slot, nameplates, floating combat text and banners.
 *  Intended to be replaced by UMG widgets once the HUD design settles.
 */
UCLASS()
class AMMOHUD : public AHUD
{
	GENERATED_BODY()

public:

	virtual void DrawHUD() override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Max distance at which creature nameplates are drawn */
	UPROPERTY(EditAnywhere, Category="HUD", meta=(Units="cm"))
	float NameplateDistance = 3500.0f;

	/** Name shown in the player frame */
	UPROPERTY(EditAnywhere, Category="HUD")
	FText PlayerDisplayName = NSLOCTEXT("MMOHUD", "PlayerName", "Adventurer");

	struct FFloatingText
	{
		FVector WorldLocation;
		FString Text;
		FLinearColor Color;
		float Age = 0.0f;
		float Duration = 1.2f;
		float Scale = 1.0f;
		float JitterX = 0.0f;
	};

	TArray<FFloatingText> FloatingTexts;

	FString ErrorMessage;
	float ErrorTime = 0.0f;

	int32 LevelUpLevel = 0;
	float LevelUpTime = 0.0f;

	float DamageFlashTime = 0.0f;

	/** Pawn whose components we are bound to */
	TWeakObjectPtr<AMMOCharacter> BoundCharacter;

	/** Scale from a 1080p reference layout */
	float UIScale = 1.0f;

	FDelegateHandle CombatEventHandle;

	void BindToCharacter(AMMOCharacter* Character);
	void UnbindFromCharacter();

	void HandleAnyCombatEvent(const UMMOHealthComponent* Component, EMMOCombatEvent Event, float Amount);

	UFUNCTION()
	void HandleCombatError(const FText& Message);

	UFUNCTION()
	void HandleLevelUp(int32 NewLevel);

	UFUNCTION()
	void HandleXPChanged(int32 CurrentXP, int32 XPToNextLevel, int32 XPGained);

	UFUNCTION()
	void HandlePlayerDamaged(float Amount, AActor* DamageInstigator);

	void AddFloatingText(const FVector& WorldLocation, const FString& Text, const FLinearColor& Color, float Scale = 1.0f, float Duration = 1.2f);

	void DrawNameplates(AMMOCharacter* Character);
	void DrawFloatingTexts(float DeltaSeconds);
	void DrawPlayerFrame(AMMOCharacter* Character);
	void DrawTargetFrame(AMMOCharacter* Character);
	void DrawXPBar(AMMOCharacter* Character);
	void DrawActionBar(AMMOCharacter* Character);
	void DrawMessages(AMMOCharacter* Character, float DeltaSeconds);
	void DrawCrosshair();
	void DrawHelp();

	/** Drawing helpers (positions in 1080p reference pixels unless noted) */
	void DrawPanel(float X, float Y, float W, float H);
	void DrawBar(float X, float Y, float W, float H, float Percent, const FLinearColor& FillColor, const FString& Label);
	void DrawLabel(const FString& Text, float X, float Y, const FLinearColor& Color, UFont* Font, float Scale, bool bCenterX = false, bool bCenterY = false);

	/** Text scale: follows UIScale but never shrinks below readable size */
	float GetTextScale() const;

	/** Projects a world location to screen pixels. Returns false if behind the camera */
	bool ProjectToScreen(const FVector& WorldLocation, FVector2D& OutScreen) const;
};

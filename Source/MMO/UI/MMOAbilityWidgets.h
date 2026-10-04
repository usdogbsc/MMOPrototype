// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "MMOAbilityWidgets.generated.h"

class AMMOCharacter;
class UBorder;
class UButton;
class UImage;
class UTextBlock;
class UVerticalBox;
class UMMOAbilityDefinition;

/** Drag payload: an ability picked up from the abilities window */
UCLASS()
class UMMOAbilityDragOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:

	FName AbilityId;
};

/** Ability tooltip: name, cast time / cooldown / range, level requirement and description */
UCLASS()
class UMMOAbilityTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetAbility(const UMMOAbilityDefinition* Ability, bool bKnown);

protected:

	UPROPERTY()
	TObjectPtr<UVerticalBox> Lines;

	virtual void NativeOnInitialized() override;
};

/** Square ability icon: texture (or initials), tooltip; drag it to the hotbar if known, click to use */
UCLASS()
class UMMOAbilityIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Setup(AMMOCharacter* InCharacter, UMMOAbilityDefinition* InAbility, float InSize);

protected:

	UPROPERTY()
	TObjectPtr<UBorder> Frame;

	UPROPERTY()
	TObjectPtr<UImage> Icon;

	UPROPERTY()
	TObjectPtr<UTextBlock> Initials;

	UPROPERTY()
	TObjectPtr<UMMOAbilityTooltipWidget> Tooltip;

	TWeakObjectPtr<AMMOCharacter> Character;
	TWeakObjectPtr<UMMOAbilityDefinition> Ability;
	float Size = 44.0f;

	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;

	bool IsKnown() const;
};

/** Abilities window (K): every ability, when it is learned, and what it does */
UCLASS()
class UMMOAbilitiesWindowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Init(AMMOCharacter* InCharacter);
	void Refresh();

	FSimpleDelegate OnCloseRequested;

protected:

	UPROPERTY()
	TObjectPtr<UVerticalBox> List;

	UPROPERTY()
	TObjectPtr<UButton> CloseButton;

	TWeakObjectPtr<AMMOCharacter> Character;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClose();
};

namespace MMOAbilityUI
{
	/** "Rending Strike" -> "RS" */
	FString Initials(const FText& Name);
}

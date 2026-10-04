// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NPC/MMONPC.h"
#include "MMONPCPlateWidget.generated.h"

class UTextBlock;

/** Friendly NPC nameplate: quest marker (! / ?), green name and <Title> */
UCLASS()
class UMMONPCPlateWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void SetPlate(const FText& Name, const FText& Title, EMMONPCMarker Marker);

	EMMONPCMarker GetMarker() const { return CurrentMarker; }

protected:

	UPROPERTY()
	TObjectPtr<UTextBlock> MarkerText;

	UPROPERTY()
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY()
	TObjectPtr<UTextBlock> TitleText;

	EMMONPCMarker CurrentMarker = EMMONPCMarker::None;
	bool bInitializedText = false;

	virtual void NativeOnInitialized() override;
};

/** A text button that reports clicks through a native delegate (for lists built in code) */
UCLASS()
class UMMOTextButtonWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void Setup(const FString& Label, const FLinearColor& Color, int32 FontSize = 13, bool bLeftAligned = false);

	FSimpleDelegate OnClickedNative;

protected:

	UPROPERTY()
	TObjectPtr<class UButton> Button;

	UPROPERTY()
	TObjectPtr<UTextBlock> Label;

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandleClicked();
};

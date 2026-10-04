// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "MMOHUD.generated.h"

class UMMOHUDWidget;

/**
 *  Owns the UMG MMO HUD for the local player.
 *  Set HUDWidgetClass to a Widget Blueprint child of UMMOHUDWidget to replace the default C++ layout.
 */
UCLASS()
class AMMOHUD : public AHUD
{
	GENERATED_BODY()

public:

	UMMOHUDWidget* GetHUDWidget() const { return HUDWidget; }

protected:

	/** HUD widget class to create. Defaults to the C++ layout */
	UPROPERTY(EditAnywhere, Category="HUD")
	TSubclassOf<UMMOHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMMOHUDWidget> HUDWidget;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};

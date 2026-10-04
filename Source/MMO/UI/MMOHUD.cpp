// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/MMOHUD.h"
#include "UI/MMOHUDWidget.h"
#include "GameFramework/PlayerController.h"
#include "MMO.h"

void AMMOHUD::BeginPlay()
{
	Super::BeginPlay();

	if (!PlayerOwner || !PlayerOwner->IsLocalController())
	{
		return;
	}

	const TSubclassOf<UMMOHUDWidget> WidgetClass = HUDWidgetClass ? HUDWidgetClass : TSubclassOf<UMMOHUDWidget>(UMMOHUDWidget::StaticClass());
	HUDWidget = CreateWidget<UMMOHUDWidget>(PlayerOwner, WidgetClass);
	if (HUDWidget)
	{
		HUDWidget->AddToViewport(0);
	}
	else
	{
		UE_LOG(LogMMO, Error, TEXT("Could not create the MMO HUD widget."));
	}
}

void AMMOHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

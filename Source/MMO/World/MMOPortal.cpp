// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOPortal.h"
#include "MMOCharacter.h"
#include "Combat/MMOCombatComponent.h"
#include "UI/MMONPCPlateWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "MMO.h"

AMMOPortal::AMMOPortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Plate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Plate"));
	Plate->SetupAttachment(Root);
	Plate->SetWidgetSpace(EWidgetSpace::Screen);
	Plate->SetWidgetClass(UMMONPCPlateWidget::StaticClass());
	Plate->SetDrawAtDesiredSize(true);
	Plate->SetPivot(FVector2D(0.5f, 1.0f));
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DisplayName = NSLOCTEXT("MMOPortal", "Mine", "Rustvein Mine");
	ActionText = NSLOCTEXT("MMOPortal", "Enter", "Enter");
}

void AMMOPortal::BeginPlay()
{
	Super::BeginPlay();
	Plate->SetRelativeLocation(FVector(0.0f, 0.0f, PlateHeight));
	MMOInteraction::Register(this);
}

void AMMOPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MMOInteraction::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

bool AMMOPortal::CanInteract(const AMMOCharacter* Player) const
{
	// no escaping mid-fight
	return Player && !Player->IsInCombat();
}

void AMMOPortal::Interact(AMMOCharacter* Player)
{
	if (!Player)
	{
		return;
	}
	Player->GetCombat()->ClearTarget();
	Player->TeleportTo(Destination, FRotator(0.0f, DestinationYaw, 0.0f), false, true);
	if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
	{
		FRotator Control = PC->GetControlRotation();
		Control.Yaw = DestinationYaw;
		PC->SetControlRotation(Control);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, 0.8f, FLinearColor::Black, false, false);
		}
	}
	UE_LOG(LogMMO, Log, TEXT("%s used %s"), *Player->GetName(), *DisplayName.ToString());
}

void AMMOPortal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	Plate->SetVisibility(Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 2000.0f);
	if (UMMONPCPlateWidget* Widget = Cast<UMMONPCPlateWidget>(Plate->GetUserWidgetObject()))
	{
		Widget->SetPlate(DisplayName, ActionText, EMMONPCMarker::None);
	}
}

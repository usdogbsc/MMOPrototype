// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOCraftingStation.h"
#include "MMOCharacter.h"
#include "UI/MMOHUD.h"
#include "UI/MMONPCPlateWidget.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMMOCraftingStation::AMMOCraftingStation()
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

	DisplayName = NSLOCTEXT("MMOCrafting", "Forge", "Forge");
}

void AMMOCraftingStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Plate->SetRelativeLocation(FVector(0.0f, 0.0f, PlateHeight));
}

void AMMOCraftingStation::BeginPlay()
{
	Super::BeginPlay();
	Plate->SetRelativeLocation(FVector(0.0f, 0.0f, PlateHeight));
	MMOInteraction::Register(this);
}

void AMMOCraftingStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MMOInteraction::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

bool AMMOCraftingStation::IsInReach(const AActor* Player) const
{
	return Player && FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= UseRange + 150.0f;
}

void AMMOCraftingStation::Interact(AMMOCharacter* Player)
{
	const APlayerController* PC = Player ? Cast<APlayerController>(Player->GetController()) : nullptr;
	if (AMMOHUD* HUD = PC ? Cast<AMMOHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->OpenCrafting(this);
	}
}

void AMMOCraftingStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	Plate->SetVisibility(Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 1500.0f);
	if (UMMONPCPlateWidget* Widget = Cast<UMMONPCPlateWidget>(Plate->GetUserWidgetObject()))
	{
		Widget->SetPlate(DisplayName, MMOProfessions::GetName(Profession), EMMONPCMarker::None);
	}
}

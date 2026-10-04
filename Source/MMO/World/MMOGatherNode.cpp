// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOGatherNode.h"
#include "MMOCharacter.h"
#include "Professions/MMOProfessionComponent.h"
#include "UI/MMONPCPlateWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace MMOGatherNodeLook
{
	static constexpr int32 NumAccents = 6;
}

AMMOGatherNode::AMMOGatherNode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.25f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	ShapeMaterial = ShapeMat.Object;

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	RootComponent = Base;
	Base->SetStaticMesh(SphereMesh.Object);
	Base->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Base->SetCanEverAffectNavigation(false);

	for (int32 Index = 0; Index < MMOGatherNodeLook::NumAccents; ++Index)
	{
		UStaticMeshComponent* Accent = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Accent%d"), Index));
		Accent->SetupAttachment(Base);
		Accent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Accent->SetCanEverAffectNavigation(false);
		Accents.Add(Accent);
	}

	Plate = CreateDefaultSubobject<UWidgetComponent>(TEXT("Plate"));
	Plate->SetupAttachment(Base);
	Plate->SetWidgetSpace(EWidgetSpace::Screen);
	Plate->SetWidgetClass(UMMONPCPlateWidget::StaticClass());
	Plate->SetDrawAtDesiredSize(true);
	Plate->SetPivot(FVector2D(0.5f, 1.0f));
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plate->SetUsingAbsoluteScale(true);

	DisplayName = NSLOCTEXT("MMOGather", "CopperVein", "Copper Vein");
}

void AMMOGatherNode::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook();
}

void AMMOGatherNode::ApplyLook()
{
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	const bool bOre = Profession == EMMOProfession::Mining;

	auto Tint = [this](UStaticMeshComponent* Component, const FLinearColor& Color)
	{
		Component->SetMaterial(0, ShapeMaterial);
		if (UMaterialInstanceDynamic* MID = Component->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};

	// base: a squat boulder for ore, a low leafy clump for herbs (the actor scale sizes the whole node)
	Base->SetStaticMesh(Sphere);
	Base->SetRelativeScale3D(bOre ? FVector(1.5f, 1.25f, 0.85f) : FVector(0.75f, 0.75f, 0.42f));
	Tint(Base, BaseColor);

	FRandomStream Random(LookSeed);
	for (int32 Index = 0; Index < Accents.Num(); ++Index)
	{
		UStaticMeshComponent* Accent = Accents[Index];
		const float Angle = (Index / static_cast<float>(Accents.Num())) * 360.0f + Random.FRandRange(-20.0f, 20.0f);
		const FVector Direction = FRotator(0.0f, Angle, 0.0f).Vector();
		if (bOre)
		{
			// chunks of ore set into the boulder's surface (positions are in the base's unscaled space: radius 50)
			Accent->SetStaticMesh(Cube);
			const float Elevation = FMath::DegreesToRadians(Random.FRandRange(5.0f, 55.0f));
			const FVector Surface = FVector(Direction.X * FMath::Cos(Elevation), Direction.Y * FMath::Cos(Elevation), FMath::Sin(Elevation)) * 47.0f;
			Accent->SetRelativeLocation(Surface);
			Accent->SetRelativeRotation(FRotator(Random.FRandRange(-40.0f, 40.0f), Random.FRandRange(0.0f, 90.0f), Random.FRandRange(-40.0f, 40.0f)));
			Accent->SetRelativeScale3D(FVector(Random.FRandRange(0.24f, 0.34f)) / FVector(1.5f, 1.25f, 0.85f));
		}
		else
		{
			// stalks with a flower on top, around and above the clump
			Accent->SetStaticMesh(Index % 2 == 0 ? Sphere : Cone);
			Accent->SetRelativeLocation(Direction * Random.FRandRange(15.0f, 45.0f) + FVector(0.0f, 0.0f, Random.FRandRange(45.0f, 80.0f)));
			Accent->SetRelativeRotation(FRotator::ZeroRotator);
			Accent->SetRelativeScale3D(FVector(Random.FRandRange(0.22f, 0.32f)) / FVector(0.75f, 0.75f, 0.42f));
		}
		Tint(Accent, AccentColor);
	}

	Plate->SetRelativeLocation(FVector(0.0f, 0.0f, bOre ? 110.0f : 150.0f));
}

void AMMOGatherNode::BeginPlay()
{
	Super::BeginPlay();
	ApplyLook();
	MMOInteraction::Register(this);
}

void AMMOGatherNode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	MMOInteraction::Unregister(this);
	Super::EndPlay(EndPlayReason);
}

void AMMOGatherNode::Interact(AMMOCharacter* Player)
{
	if (Player && !bDepleted)
	{
		Player->GetProfessions()->StartGather(this);
	}
}

void AMMOGatherNode::SetShown(bool bShown)
{
	SetActorHiddenInGame(!bShown);
	SetActorEnableCollision(bShown);
}

void AMMOGatherNode::Deplete()
{
	if (bDepleted)
	{
		return;
	}
	bDepleted = true;
	SetShown(false);
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &AMMOGatherNode::Respawn, FMath::Max(0.1f, RespawnTime), false);
}

void AMMOGatherNode::Respawn()
{
	bDepleted = false;
	SetShown(true);
}

void AMMOGatherNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bNear = !bDepleted && Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) < 1800.0f;
	Plate->SetVisibility(bNear);
	if (UMMONPCPlateWidget* Widget = Cast<UMMONPCPlateWidget>(Plate->GetUserWidgetObject()))
	{
		const FText Title = RequiredSkill > 1
			? FText::Format(NSLOCTEXT("MMOGather", "TitleSkill", "{0} {1}"), MMOProfessions::GetName(Profession), RequiredSkill)
			: MMOProfessions::GetName(Profession);
		Widget->SetPlate(DisplayName, Title, EMMONPCMarker::None);
	}
}

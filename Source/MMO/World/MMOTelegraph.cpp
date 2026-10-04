// Copyright Epic Games, Inc. All Rights Reserved.

#include "World/MMOTelegraph.h"
#include "MMOCharacter.h"
#include "Combat/MMOHealthComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

FMMOTelegraphDetonated AMMOTelegraph::OnAnyDetonation;

AMMOTelegraph::AMMOTelegraph()
{
	PrimaryActorTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	FillMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MMO/World/Materials/MI_MMO_glow_danger.MI_MMO_glow_danger")));
	RimMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/MMO/World/Materials/MI_MMO_glow_ember.MI_MMO_glow_ember")));
	EruptSoundAsset = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/MMO/Audio/S_MMO_Eruption.S_MMO_Eruption")));

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	RootComponent = Disc;
	Disc->SetStaticMesh(CylinderMesh.Object);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);

	Rim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Rim"));
	Rim->SetupAttachment(Disc);
	Rim->SetStaticMesh(CylinderMesh.Object);
	Rim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rim->SetCastShadow(false);
	Rim->SetUsingAbsoluteScale(true);

}

void AMMOTelegraph::Arm(float InRadius, float InDelay, float InDamage, AActor* InSource)
{
	Disc->SetMaterial(0, FillMaterial.LoadSynchronous());
	Rim->SetMaterial(0, RimMaterial.LoadSynchronous());
	EruptSound = EruptSoundAsset.LoadSynchronous();

	Radius = InRadius;
	Delay = FMath::Max(0.1f, InDelay);
	Damage = InDamage;
	Source = InSource;
	Age = 0.0f;
	bDetonated = false;

	// the outer ring shows the full danger zone from the start; the inner disc grows until it erupts
	Rim->SetWorldScale3D(FVector(Radius * 2.0f / 100.0f, Radius * 2.0f / 100.0f, 0.02f));
	Rim->SetRelativeLocation(FVector(0.0f, 0.0f, -0.5f));
	SetActorScale3D(FVector(0.1f, 0.1f, 0.03f));
}

void AMMOTelegraph::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (!bDetonated)
	{
		const float T = FMath::Clamp(Age / Delay, 0.0f, 1.0f);
		const float Size = Radius * 2.0f / 100.0f * FMath::Lerp(0.1f, 1.0f, T);
		SetActorScale3D(FVector(Size, Size, 0.03f));
		if (Age >= Delay)
		{
			Detonate();
		}
		return;
	}

	// flash, then sink away
	const float Fade = Age - Delay;
	SetActorScale3D(FVector(Radius * 2.0f / 100.0f * (1.0f + Fade * 0.3f), Radius * 2.0f / 100.0f * (1.0f + Fade * 0.3f), 0.03f + FMath::Max(0.0f, 0.6f - Fade) * 0.8f));
	if (Fade > 0.7f)
	{
		Destroy();
	}
}

void AMMOTelegraph::Detonate()
{
	bDetonated = true;
	Rim->SetVisibility(false);

	TArray<AActor*> Hit;
	for (TActorIterator<AMMOCharacter> It(GetWorld()); It; ++It)
	{
		AMMOCharacter* Player = *It;
		if (Player->IsDead())
		{
			continue;
		}
		const float Reach = Radius + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.5f;
		if (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) <= Reach && FMath::Abs(Player->GetActorLocation().Z - GetActorLocation().Z) < 300.0f)
		{
			Player->GetHealth()->ApplyDamage(Damage, Source.Get());
			Hit.Add(Player);
		}
	}
	if (EruptSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, EruptSound, GetActorLocation());
	}
	OnAnyDetonation.Broadcast(this, Hit);
}

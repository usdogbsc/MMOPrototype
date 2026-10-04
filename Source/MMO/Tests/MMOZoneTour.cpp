// Copyright Epic Games, Inc. All Rights Reserved.

// Development-only visual tour of the Thornwick zone:
//   mmo.tour        teleports the player through the zone's key viewpoints, saving a screenshot at each
//   mmo.tour quit   ...and exits when done (add viewpoint names, e.g. "mmo.tour Hollis Doran", to visit only those)
//   mmo.goto <Name> teleports to one viewpoint (Village, Gate, Meadow, Tower, Stones, Woods, Camp, Den, Waterfall, Hollow, Mine, Overview, Hollis, Brenna, Doran, Pell, Copper, Herbs, MineHall, MineNest, MineThrone)
// The player is invulnerable during the tour.

#include "CoreMinimal.h"
#include "Misc/App.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "GameFramework/PlayerController.h"
#include "MMOCharacter.h"
#include "Combat/MMOHealthComponent.h"
#include "World/MMOTerrain.h"
#include "MMO.h"

namespace MMOZoneTour
{
	struct FViewpoint
	{
		const TCHAR* Name;
		float X, Y, Yaw, Pitch, Zoom;
		/** Absolute floor height (indoor viewpoints); terrain height is used when unset */
		float FloorZ = -100000.0f;
	};

	static const FViewpoint Viewpoints[] =
	{
		{ TEXT("Village"),   -1500.0f,  -250.0f,    10.0f, -14.0f,  700.0f },
		{ TEXT("Square"),     1500.0f,  1200.0f,  -150.0f, -18.0f,  900.0f },
		{ TEXT("Gate"),       2600.0f,   150.0f,     0.0f, -10.0f,  650.0f },
		{ TEXT("Meadow"),     5200.0f,   600.0f,     8.0f, -12.0f,  900.0f },
		{ TEXT("Tower"),      7300.0f,  3400.0f,    60.0f, -10.0f,  900.0f },
		{ TEXT("Stones"),     5600.0f, -4600.0f,   -55.0f, -14.0f,  900.0f },
		{ TEXT("Wagon"),     10400.0f,   200.0f,    20.0f, -12.0f,  800.0f },
		{ TEXT("Woods"),     13300.0f,   700.0f,    15.0f, -10.0f,  650.0f },
		{ TEXT("Camp"),      13800.0f,  2500.0f,    30.0f, -14.0f,  800.0f },
		{ TEXT("Den"),       15200.0f, -3900.0f,   -30.0f, -10.0f,  800.0f },
		{ TEXT("Waterfall"), 13650.0f, -5500.0f,   -90.0f,  -4.0f,  900.0f },
		{ TEXT("Hollow"),    19450.0f,  4950.0f,    40.0f, -12.0f,  900.0f },
		{ TEXT("Mine"),      19900.0f, -1200.0f,   -15.0f,  -8.0f,  800.0f },
		{ TEXT("Overview"),   -400.0f,   400.0f,     5.0f, -32.0f, 1100.0f },
		{ TEXT("Hollis"),     3100.0f,   150.0f,  -152.0f,  -8.0f,  450.0f },
		{ TEXT("Brenna"),       -1.0f,  -130.0f,  -156.0f,  -8.0f,  450.0f },
		{ TEXT("Doran"),      1180.0f,   178.0f,    11.0f,  -8.0f,  450.0f },
		{ TEXT("Pell"),        389.0f,   -94.0f,   -46.0f,  -8.0f,  450.0f },
		{ TEXT("Copper"),     7250.0f,  4150.0f,    45.0f, -14.0f,  420.0f },
		{ TEXT("MineHall"),  30250.0f, -3000.0f,     0.0f, -10.0f,  500.0f, 0.0f },
		{ TEXT("MineNest"),  30300.0f, -3000.0f,     0.0f,  -8.0f,  600.0f, 0.0f },
		{ TEXT("MineThrone"), 35700.0f,   -100.0f,    0.0f, -10.0f,  700.0f, 0.0f },
		{ TEXT("Herbs"),      4350.0f, -1250.0f,    50.0f, -18.0f,  380.0f },
	};

	struct FTour
	{
		TWeakObjectPtr<UWorld> World;
		int32 Index = 0;
		/** Viewpoint names to visit (empty = all) */
		TArray<FString> Only;
		double StepStart = 0.0;
		bool bShot = false;
		/** Frame time sampling between arriving and the screenshot (performance pass) */
		double FrameTimeSum = 0.0;
		int32 Frames = 0;
		bool bQuit = false;
		FTSTicker::FDelegateHandle Ticker;
	};

	static TUniquePtr<FTour> Tour;

	static bool GoTo(UWorld* World, const FViewpoint& View)
	{
		AMMOCharacter* Player = Cast<AMMOCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		const AMMOTerrain* Terrain = AMMOTerrain::Find(World);
		if (!Player)
		{
			return false;
		}

		const float Z = (View.FloorZ > -90000.0f ? View.FloorZ : (Terrain ? Terrain->GetHeightAt(View.X, View.Y) : 0.0f)) + 110.0f;
		Player->TeleportTo(FVector(View.X, View.Y, Z), FRotator(0.0f, View.Yaw, 0.0f), false, true);
		if (AController* Controller = Player->GetController())
		{
			Controller->SetControlRotation(FRotator(View.Pitch, View.Yaw, 0.0f));
		}
		Player->DoZoom((Player->GetDesiredCameraDistance() - View.Zoom) / 75.0f);
		return true;
	}

	static bool Tick(float DeltaTime)
	{
		UWorld* World = Tour ? Tour->World.Get() : nullptr;
		if (!World)
		{
			Tour.Reset();
			return false;
		}

		const double Now = World->GetTimeSeconds();
		if (Tour->Index >= UE_ARRAY_COUNT(Viewpoints))
		{
			UE_LOG(LogMMO, Display, TEXT("MMO TOUR COMPLETE"));
			const bool bQuit = Tour->bQuit;
			Tour.Reset();
			if (bQuit)
			{
				FPlatformMisc::RequestExit(false, TEXT("MMOZoneTour"));
			}
			return false;
		}

		const FViewpoint& View = Viewpoints[Tour->Index];
		if (Tour->Only.Num() > 0 && !Tour->Only.Contains(View.Name))
		{
			++Tour->Index;
			return true;
		}
		if (Tour->StepStart > 0.0 && Now - Tour->StepStart > 1.0 && !Tour->bShot)
		{
			// skip the first second after teleporting (streaming, shader warm-up)
			Tour->FrameTimeSum += FApp::GetDeltaTime();
			++Tour->Frames;
		}
		if (Tour->StepStart == 0.0)
		{
			GoTo(World, View);
			Tour->StepStart = Now;
			Tour->bShot = false;
		}
		else if (!Tour->bShot && Now - Tour->StepStart > 2.5)
		{
			const double AverageMs = Tour->Frames > 0 ? Tour->FrameTimeSum / Tour->Frames * 1000.0 : 0.0;
			UE_LOG(LogMMO, Display, TEXT("MMO TOUR perf %s: %.1f ms/frame (%.0f fps) over %d frames"), View.Name, AverageMs, AverageMs > 0.0 ? 1000.0 / AverageMs : 0.0, Tour->Frames);
			Tour->FrameTimeSum = 0.0;
			Tour->Frames = 0;
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("MMOTour_%02d_%s.png"), Tour->Index, View.Name), true, false);
			UE_LOG(LogMMO, Display, TEXT("MMO TOUR shot %s"), View.Name);
			Tour->bShot = true;
		}
		else if (Tour->bShot && Now - Tour->StepStart > 3.2)
		{
			++Tour->Index;
			Tour->StepStart = 0.0;
		}
		return true;
	}

	static void Run(const TArray<FString>& Args, UWorld* World)
	{
		if (Tour)
		{
			return;
		}
		if (AMMOCharacter* Player = Cast<AMMOCharacter>(UGameplayStatics::GetPlayerPawn(World, 0)))
		{
			Player->GetHealth()->SetInvulnerable(true);
		}
		Tour = MakeUnique<FTour>();
		Tour->World = World;
		Tour->bQuit = Args.Contains(TEXT("quit"));
		for (const FViewpoint& View : Viewpoints)
		{
			if (Args.Contains(View.Name))
			{
				Tour->Only.Add(View.Name);
			}
		}
		Tour->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.0f);
	}

	static void GoToCommand(const TArray<FString>& Args, UWorld* World)
	{
		for (const FViewpoint& View : Viewpoints)
		{
			if (Args.Num() > 0 && Args[0].Equals(View.Name, ESearchCase::IgnoreCase))
			{
				GoTo(World, View);
				return;
			}
		}
		UE_LOG(LogMMO, Warning, TEXT("mmo.goto: unknown place. Try Village, Square, Gate, Meadow, Tower, Stones, Wagon, Woods, Camp, Den, Waterfall, Hollow, Mine, Overview, Hollis, Brenna, Doran, Pell, Copper, Herbs"));
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMMOTourCommand(TEXT("mmo.tour"), TEXT("Visual tour of the zone with screenshots ('quit' to exit after)."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOZoneTour::Run));

static FAutoConsoleCommandWithWorldAndArgs GMMOGotoCommand(TEXT("mmo.goto"), TEXT("mmo.goto <Place>: teleport to a zone viewpoint."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOZoneTour::GoToCommand));

static FAutoConsoleCommand GMMOQuitAfterCommand(TEXT("mmo.quitafter"), TEXT("mmo.quitafter <seconds>: exits the game cleanly after a delay (for scripted test sessions)."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const float Delay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 5.0f;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			FPlatformMisc::RequestExit(false, TEXT("mmo.quitafter"));
			return false;
		}), Delay);
	}));

static FAutoConsoleCommand GMMOScreenshotCommand(TEXT("mmo.screenshot"), TEXT("mmo.screenshot <delay seconds> <name>: saves Saved/Screenshots/<name>.png after a delay."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		const float Delay = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 1.0f;
		const FString Name = Args.Num() > 1 ? Args[1] : TEXT("MMOShot");
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Name](float)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / (Name + TEXT(".png")), true, false);
			return false;
		}), Delay);
	}));

#endif // !UE_BUILD_SHIPPING

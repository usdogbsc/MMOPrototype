// Copyright Epic Games, Inc. All Rights Reserved.

// Development-only end-to-end check of the Milestone 1A loop, run inside a live game world:
//   UnrealEditor.exe MMO.uproject Lvl_ThirdPerson -game -ExecCmds="mmo.selftest"
// It drives the real player/wolf objects and logs PASS/FAIL per step.
// Options: "quit" exits when finished, "shots" saves HUD screenshots to Saved/Screenshots (needs rendering).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "MMOCharacter.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Creatures/MMOCreature.h"
#include "Creatures/MMOCreatureAIController.h"
#include "UI/MMOHUD.h"
#include "MMO.h"

namespace MMOSelfTest
{
	struct FState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AMMOCharacter> Player;
		TWeakObjectPtr<AMMOCreature> Wolf;
		int32 Step = 0;
		double StepStart = 0.0;
		int32 Failures = 0;
		int32 Passes = 0;
		bool bQuitWhenDone = false;
		bool bScreenshots = false;
		FVector SafeOrigin = FVector::ZeroVector;
		float OriginalLeashRange = 0.0f;
		FTSTicker::FDelegateHandle Ticker;
	};

	static TUniquePtr<FState> State;

	static double Now() { return State->World.IsValid() ? State->World->GetTimeSeconds() : 0.0; }

	static void Check(bool bCondition, const FString& What)
	{
		if (bCondition)
		{
			++State->Passes;
			UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST PASS: %s"), *What);
		}
		else
		{
			++State->Failures;
			UE_LOG(LogMMO, Error, TEXT("MMO SELFTEST FAIL: %s"), *What);
		}
	}

	static void Shot(const TCHAR* Name)
	{
		if (State->bScreenshots)
		{
			FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("MMOSelfTest_%s.png"), Name), true, false);
		}
	}

	static void NextStep()
	{
		++State->Step;
		State->StepStart = Now();
	}

	static float Elapsed() { return static_cast<float>(Now() - State->StepStart); }

	static void PlacePlayerNear(const FVector& Location, float Distance)
	{
		// walk from the wolf back toward the player start, which is known to be on the floor
		AMMOCharacter* Player = State->Player.Get();
		const FVector Direction = (State->SafeOrigin - Location).GetSafeNormal2D();
		FVector Destination = Location + Direction * Distance;
		Destination.Z = FMath::Max(Location.Z, State->SafeOrigin.Z) + 30.0f;
		Player->TeleportTo(Destination, (-Direction).Rotation());
		if (AController* Controller = Player->GetController())
		{
			Controller->SetControlRotation(FRotator(-15.0f, (-Direction).Rotation().Yaw, 0.0f));
		}
	}

	static EMMOCreatureAIState WolfState()
	{
		const AMMOCreatureAIController* AI = State->Wolf.IsValid() ? Cast<AMMOCreatureAIController>(State->Wolf->GetController()) : nullptr;
		return AI ? AI->GetAIState() : EMMOCreatureAIState::Dead;
	}

	static void Finish()
	{
		UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST COMPLETE: %d passed, %d failed -> %s"),
			State->Passes, State->Failures, State->Failures == 0 ? TEXT("SUCCESS") : TEXT("FAILURE"));

		FTSTicker::GetCoreTicker().RemoveTicker(State->Ticker);
		const bool bQuit = State->bQuitWhenDone;
		State.Reset();

		if (bQuit)
		{
			FPlatformMisc::RequestExit(false, TEXT("MMOSelfTest"));
		}
	}

	static bool Tick(float DeltaTime)
	{
		if (!State)
		{
			return false;
		}

		AMMOCharacter* Player = State->Player.Get();
		AMMOCreature* Wolf = State->Wolf.Get();
		if (!State->World.IsValid() || !Player || !Wolf)
		{
			Check(false, TEXT("World, player and wolf stay valid"));
			Finish();
			return false;
		}

		UMMOHealthComponent* PlayerHealth = Player->GetHealth();
		UMMOCombatComponent* Combat = Player->GetCombat();
		UMMOProgressionComponent* Progression = Player->GetProgression();
		UMMOHealthComponent* WolfHealth = Wolf->GetHealth();
		const FVector WolfHome = Wolf->GetSpawnTransform().GetLocation();

		switch (State->Step)
		{
		case 0: // initial state
			Check(Progression->GetLevel() == 1, TEXT("Player starts at level 1"));
			Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 100.0f) && FMath::IsNearlyEqual(PlayerHealth->GetMaxHealth(), 100.0f), TEXT("Player starts at 100/100 health"));
			Check(Progression->GetCurrentXP() == 0 && Progression->GetXPToNextLevel() == 100, TEXT("Player starts at 0/100 XP"));
			Check(Wolf->GetTargetDisplayName().ToString() == TEXT("Grey Wolf"), TEXT("Creature is named Grey Wolf"));
			Check(WolfState() == EMMOCreatureAIState::Idle, TEXT("Wolf starts idle"));
			Check(Cast<AMMOHUD>(Cast<APlayerController>(Player->GetController())->GetHUD()) != nullptr, TEXT("MMO HUD is active"));
			PlacePlayerNear(WolfHome, 600.0f);
			NextStep();
			break;

		case 1: // aggro + chase
			if (Elapsed() > 0.5f)
			{
				const EMMOCreatureAIState AIState = WolfState();
				Check(AIState == EMMOCreatureAIState::Chasing || AIState == EMMOCreatureAIState::Attacking, TEXT("Wolf aggros when the player enters its aggro range"));
				NextStep();
			}
			break;

		case 2: // wolf reaches and bites the player
			if (PlayerHealth->GetCurrentHealth() < PlayerHealth->GetMaxHealth())
			{
				Check(FVector::Dist2D(Wolf->GetActorLocation(), WolfHome) > 50.0f, TEXT("Wolf chased toward the player"));
				Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), PlayerHealth->GetMaxHealth() - Wolf->AttackDamage), TEXT("Wolf attack damaged the player by its Attack Damage"));
				NextStep();
			}
			else if (Elapsed() > 8.0f)
			{
				Check(false, TEXT("Wolf attacked the player within 8s"));
				NextStep();
			}
			break;

		case 3: // targeting + cooldown
		{
			Combat->SetTarget(Wolf);
			Check(Combat->GetCurrentTarget() == Wolf, TEXT("Player can target the wolf"));
			const float Before = WolfHealth->GetCurrentHealth();
			Check(Combat->TryBasicAttack() == EMMOAttackResult::Success, TEXT("Basic Attack succeeds in range"));
			Check(FMath::IsNearlyEqual(WolfHealth->GetCurrentHealth(), Before - Combat->BasicAttackDamage), TEXT("Basic Attack damages the wolf"));
			Check(Combat->TryBasicAttack() == EMMOAttackResult::OnCooldown, TEXT("Basic Attack respects its cooldown"));
			Shot(TEXT("1_Combat"));
			NextStep();
			break;
		}

		case 4: // fight to the death
			if (WolfHealth->IsDead())
			{
				Check(Wolf->IsDead() && !Wolf->IsTargetable(), TEXT("Dead wolf is not targetable"));
				Check(WolfState() == EMMOCreatureAIState::Dead, TEXT("Dead wolf AI stops"));
				Check(Progression->GetCurrentXP() == Wolf->XPReward, TEXT("Killing the wolf awards its XP"));
				Check(Combat->TryBasicAttack() == EMMOAttackResult::TargetDead, TEXT("Cannot attack a dead target"));
				WolfHealth->ApplyDamage(10.0f, Player);
				Check(Progression->GetCurrentXP() == Wolf->XPReward, TEXT("XP is awarded exactly once"));

				// step out of aggro range so the respawned wolf stays idle
				PlacePlayerNear(WolfHome, 1500.0f);
				NextStep();
			}
			else if (Elapsed() > 20.0f)
			{
				Check(false, TEXT("Wolf killed within 20s"));
				NextStep();
			}
			else if (Combat->GetBasicAttackCooldownRemaining() <= 0.0f)
			{
				Combat->SetTarget(Wolf);
				Combat->TryBasicAttack();
			}
			break;

		case 5: // corpse removed, then respawn
			if (!Wolf->IsDead())
			{
				Check(Elapsed() >= Wolf->RespawnDelay - 0.5f, TEXT("Wolf respawns after its respawn delay"));
				Check(WolfHealth->GetCurrentHealth() == WolfHealth->GetMaxHealth(), TEXT("Respawned wolf has full health"));
				Check(Wolf->IsTargetable(), TEXT("Respawned wolf is targetable"));
				Check(FVector::Dist2D(Wolf->GetActorLocation(), WolfHome) < 10.0f, TEXT("Respawned wolf is at its spawn point"));
				Check(Combat->GetCurrentTarget() == nullptr, TEXT("Target cleared once the corpse despawned"));
				NextStep();
			}
			else if (Elapsed() > Wolf->RespawnDelay + 3.0f)
			{
				Check(false, TEXT("Wolf respawned in time"));
				NextStep();
			}
			break;

		case 6: // XP overflow + level up
		{
			Progression->AddXP(90);
			Check(Progression->GetLevel() == 2, TEXT("Level up at 100 XP"));
			Check(Progression->GetCurrentXP() == Wolf->XPReward + 90 - 100, TEXT("XP overflow carried into level 2"));
			Check(Progression->GetXPToNextLevel() == 150, TEXT("Level 2 requires 150 XP"));
			Check(FMath::IsNearlyEqual(PlayerHealth->GetMaxHealth(), 110.0f) && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 110.0f), TEXT("Level up raises max health and heals"));
			Check(FMath::IsNearlyEqual(Combat->BasicAttackDamage, 14.0f), TEXT("Level up raises Basic Attack damage"));
			NextStep();
			break;
		}

		case 7: // let the level-up banner settle in for the screenshot
		{
			if (Elapsed() > 0.4f)
			{
				Shot(TEXT("2_LevelUp"));
				NextStep();
			}
			break;
		}

		case 8: // out of range check, then pull and leash
			Combat->SetTarget(Wolf);
			PlacePlayerNear(WolfHome, 1500.0f);
			NextStep();
			break;

		case 9:
			if (Elapsed() > 0.2f)
			{
				Check(Combat->TryBasicAttack() == EMMOAttackResult::OutOfRange, TEXT("Basic Attack fails when out of range"));
				Check(WolfState() == EMMOCreatureAIState::Idle, TEXT("Wolf ignores a player outside aggro range"));
				PlacePlayerNear(WolfHome, 700.0f);
				NextStep();
			}
			break;

		case 10:
			if (WolfState() == EMMOCreatureAIState::Chasing || WolfState() == EMMOCreatureAIState::Attacking)
			{
				// shrink the leash so the chase exceeds it without leaving the test area
				State->OriginalLeashRange = Wolf->LeashRange;
				Wolf->LeashRange = 200.0f;
				NextStep();
			}
			else if (Elapsed() > 3.0f)
			{
				Check(false, TEXT("Wolf re-aggroed for leash test"));
				NextStep();
			}
			break;

		case 11:
			if (WolfState() == EMMOCreatureAIState::Returning)
			{
				Check(true, TEXT("Wolf leashes and returns when pulled too far from spawn"));
				Check(WolfHealth->ApplyDamage(5.0f, Player) == 0.0f, TEXT("Returning wolf evades damage"));
				Wolf->LeashRange = State->OriginalLeashRange;
				PlacePlayerNear(WolfHome, 1500.0f);
				NextStep();
			}
			else if (Elapsed() > 15.0f)
			{
				Check(false, TEXT("Wolf leashed within 15s"));
				NextStep();
			}
			break;

		case 12:
			if (WolfState() == EMMOCreatureAIState::Idle)
			{
				Check(FVector::Dist2D(Wolf->GetActorLocation(), WolfHome) < 100.0f, TEXT("Wolf resets at its spawn point"));
				Check(WolfHealth->GetCurrentHealth() == WolfHealth->GetMaxHealth() && !WolfHealth->IsInvulnerable(), TEXT("Reset wolf has full health and is attackable"));
				NextStep();
			}
			else if (Elapsed() > 15.0f)
			{
				Check(false, TEXT("Wolf got home within 15s"));
				NextStep();
			}
			break;

		case 13: // player death + respawn
			PlayerHealth->ApplyDamage(10000.0f, Wolf);
			Check(Player->IsDead() && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 0.0f), TEXT("Player dies at zero health (never below)"));
			Check(Combat->TryBasicAttack() == EMMOAttackResult::AttackerDead, TEXT("Dead player cannot attack"));
			Shot(TEXT("3_Death"));
			NextStep();
			break;

		case 14:
			if (!Player->IsDead())
			{
				Check(Elapsed() >= Player->GetRespawnDelay() - 0.5f, TEXT("Player respawns after the respawn delay"));
				Check(PlayerHealth->GetCurrentHealth() == PlayerHealth->GetMaxHealth(), TEXT("Respawned player has full health"));
				Check(Player->InputEnabled(), TEXT("Respawned player has control"));
				Check(Progression->GetLevel() == 2, TEXT("Level is kept through death"));
				Finish();
				return false;
			}
			else if (Elapsed() > Player->GetRespawnDelay() + 3.0f)
			{
				Check(false, TEXT("Player respawned in time"));
				Finish();
				return false;
			}
			break;
		}

		return true;
	}

	static void Run(const TArray<FString>& Args, UWorld* World)
	{
		if (State)
		{
			UE_LOG(LogMMO, Warning, TEXT("MMO SELFTEST already running"));
			return;
		}

		State = MakeUnique<FState>();
		State->World = World;
		State->bQuitWhenDone = Args.Contains(TEXT("quit"));
		State->bScreenshots = Args.Contains(TEXT("shots"));
		State->Player = Cast<AMMOCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
		State->SafeOrigin = State->Player.IsValid() ? State->Player->GetActorLocation() : FVector::ZeroVector;

		// pick the wolf nearest the player start
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AMMOCreature> It(World); It; ++It)
		{
			const float Distance = State->Player.IsValid() ? FVector::Dist(It->GetActorLocation(), State->Player->GetActorLocation()) : 0.0f;
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				State->Wolf = *It;
			}
		}

		UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST START (player: %s, wolf: %s)"), *GetNameSafe(State->Player.Get()), *GetNameSafe(State->Wolf.Get()));
		State->StepStart = Now();
		State->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.0f);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMMOSelfTestCommand(
	TEXT("mmo.selftest"),
	TEXT("Runs the Milestone 1A gameplay loop self-test in the current world. Pass 'quit' to exit when finished."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOSelfTest::Run));

#endif // !UE_BUILD_SHIPPING

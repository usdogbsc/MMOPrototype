// Copyright Epic Games, Inc. All Rights Reserved.

// Development-only end-to-end check of the combat loop (Milestones 1A + 1B), run inside a live game world:
//   UnrealEditor.exe MMO.uproject -game -ExecCmds="mmo.selftest"
// It drives the real player/wolf objects and logs PASS/FAIL per step.
// Options: "quit" exits when finished, "shots" saves HUD screenshots to Saved/Screenshots (needs rendering).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "MMOCharacter.h"
#include "Combat/MMOCombatComponent.h"
#include "Combat/MMOHealthComponent.h"
#include "Combat/MMOProgressionComponent.h"
#include "Creatures/MMOCreature.h"
#include "Creatures/MMOCreatureAIController.h"
#include "UI/MMOHUD.h"
#include "UI/MMOHUDWidget.h"
#include "MMO.h"

namespace MMOSelfTest
{
	struct FState
	{
		TWeakObjectPtr<UWorld> World;
		TWeakObjectPtr<AMMOCharacter> Player;
		TWeakObjectPtr<AMMOCreature> Wolf;
		TArray<TWeakObjectPtr<AMMOCreature>> Pack;
		int32 Step = 0;
		double StepStart = 0.0;
		int32 Failures = 0;
		int32 Passes = 0;
		bool bQuitWhenDone = false;
		bool bScreenshots = false;
		FVector SafeOrigin = FVector::ZeroVector;
		float OriginalLeashRange = 0.0f;
		double Mark = 0.0;
		float HealthMark = 0.0f;
		float OtherHealthMark = 0.0f;
		int32 CounterMark = 0;
		bool bFlag = false;
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

	/**
	 *  Places the player Distance away from Location, preferably toward the player start (known floor).
	 *  If level geometry blocks that spot, other directions around Location are tried.
	 */
	static void PlacePlayerNear(const FVector& Location, float Distance)
	{
		AMMOCharacter* Player = State->Player.Get();
		const FVector Preferred = (State->SafeOrigin - Location).GetSafeNormal2D();

		for (int32 Attempt = 0; Attempt < 12; ++Attempt)
		{
			// 0, +30, -30, +60, -60 ... degrees from the preferred direction
			const float Angle = ((Attempt + 1) / 2) * 30.0f * (Attempt % 2 == 0 ? 1.0f : -1.0f);
			const FVector Direction = Preferred.RotateAngleAxis(Angle, FVector::UpVector);
			FVector Destination = Location + Direction * Distance;
			Destination.Z = FMath::Max(Location.Z, State->SafeOrigin.Z) + 30.0f;

			if (Player->TeleportTo(Destination, (-Direction).Rotation()))
			{
				if (AController* Controller = Player->GetController())
				{
					Controller->SetControlRotation(FRotator(-15.0f, (-Direction).Rotation().Yaw, 0.0f));
				}
				UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST: placed player %.0fcm from %s (attempt %d)"), FVector::Dist2D(Player->GetActorLocation(), Location), *Location.ToCompactString(), Attempt + 1);
				return;
			}
		}

		UE_LOG(LogMMO, Warning, TEXT("MMO SELFTEST: could not place player %.0fcm from %s"), Distance, *Location.ToCompactString());
	}

	static AMMOCreatureAIController* AIOf(const AMMOCreature* Creature)
	{
		return Creature ? Cast<AMMOCreatureAIController>(Creature->GetController()) : nullptr;
	}

	static EMMOCreatureAIState StateOf(const AMMOCreature* Creature)
	{
		const AMMOCreatureAIController* AI = AIOf(Creature);
		return AI ? AI->GetAIState() : EMMOCreatureAIState::Dead;
	}

	static bool InCombat(const AMMOCreature* Creature)
	{
		const EMMOCreatureAIState S = StateOf(Creature);
		return S == EMMOCreatureAIState::Chasing || S == EMMOCreatureAIState::Attacking;
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

		UWorld* World = State->World.Get();
		UMMOHealthComponent* PlayerHealth = Player->GetHealth();
		UMMOCombatComponent* Combat = Player->GetCombat();
		UMMOProgressionComponent* Progression = Player->GetProgression();
		UMMOHealthComponent* WolfHealth = Wolf->GetHealth();
		const FVector WolfHome = Wolf->GetSpawnTransform().GetLocation();

		switch (State->Step)
		{
		case 0: // wait for the runtime NavMesh, then check the starting state
		{
			UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
			if (NavSys && NavSys->IsNavigationBuildInProgress() && Elapsed() < 20.0f)
			{
				break;
			}

			Check(Progression->GetLevel() == 1, TEXT("Player starts at level 1"));
			Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 100.0f) && FMath::IsNearlyEqual(PlayerHealth->GetMaxHealth(), 100.0f), TEXT("Player starts at 100/100 health"));
			Check(Progression->GetCurrentXP() == 0 && Progression->GetXPToNextLevel() == 100, TEXT("Player starts at 0/100 XP"));
			Check(Wolf->GetTargetDisplayName().ToString() == TEXT("Grey Wolf"), TEXT("Creature is named Grey Wolf"));
			Check(State->Pack.Num() >= 2, TEXT("Several wolves exist (a lone wolf plus a pair)"));
			Check(StateOf(Wolf) == EMMOCreatureAIState::Idle, TEXT("Wolf starts idle"));

			const AMMOHUD* HUD = Cast<AMMOHUD>(Cast<APlayerController>(Player->GetController())->GetHUD());
			Check(HUD && HUD->GetHUDWidget() && HUD->GetHUDWidget()->IsInViewport() && HUD->GetHUDWidget()->HasFrames(), TEXT("UMG HUD is in the viewport with player and target frames"));

			Check(NavSys && NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) != nullptr, TEXT("NavMesh exists in the test map"));
			const UNavigationPath* Path = NavSys ? NavSys->FindPathToLocationSynchronously(World, WolfHome, State->SafeOrigin) : nullptr;
			Check(Path && Path->IsValid() && !Path->IsPartial(), TEXT("NavMesh path exists from the wolf to the player start"));

			// camera zoom limits
			const float Start = Player->GetDesiredCameraDistance();
			Player->DoZoom(-2.0f);
			Check(Player->GetDesiredCameraDistance() > Start, TEXT("Mouse wheel zooms the camera out"));
			Player->DoZoom(100.0f);
			const float Min = Player->GetDesiredCameraDistance();
			Player->DoZoom(-100.0f);
			const float Max = Player->GetDesiredCameraDistance();
			Check(Min >= 200.0f && Max <= 1500.0f && Max > Min + 400.0f, FString::Printf(TEXT("Camera zoom is clamped to a sensible range (%.0f-%.0f)"), Min, Max));
			Player->DoZoom((Max - Start) / 75.0f);

			PlacePlayerNear(WolfHome, 600.0f);
			NextStep();
			break;
		}

		case 1: // aggro + NavMesh chase
			if (Elapsed() > 0.4f)
			{
				Check(InCombat(Wolf), TEXT("Wolf aggros when the player enters its aggro range"));
				Check(AIOf(Wolf) && AIOf(Wolf)->IsUsingNavigation(), TEXT("Wolf chases using NavMesh pathfinding"));
				Check(FMath::Abs(Player->GetCameraBoom()->TargetArmLength - Player->GetDesiredCameraDistance()) < 40.0f, TEXT("Camera eased to the requested zoom distance"));
				NextStep();
			}
			break;

		case 2: // wolf bite is timed by its attack animation
			if (PlayerHealth->GetCurrentHealth() < PlayerHealth->GetMaxHealth())
			{
				Check(Now() - Wolf->GetLastAttackTime() >= 0.2, FString::Printf(TEXT("Wolf bite lands on the attack's hit frame (%.2fs after the attack starts)"), Now() - Wolf->GetLastAttackTime()));
				Check(FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), PlayerHealth->GetMaxHealth() - Wolf->AttackDamage), TEXT("Wolf bite deals its Attack Damage"));
				NextStep();
			}
			else if (Elapsed() > 8.0f)
			{
				Check(false, TEXT("Wolf attacked the player within 8s"));
				NextStep();
			}
			break;

		case 3: // target + auto-attack on
			Combat->SetTarget(Wolf);
			Check(Combat->GetCurrentTarget() == Wolf, TEXT("Player can target the wolf"));
			Check(Combat->StartAutoAttack() == EMMOAttackResult::Success && Combat->IsAutoAttacking(), TEXT("Auto-attack activates on a living target in range"));
			State->HealthMark = WolfHealth->GetCurrentHealth();
			NextStep();
			break;

		case 4: // swing starts without dealing damage yet
			if (Combat->IsSwingPending())
			{
				State->Mark = Now();
				Check(WolfHealth->GetCurrentHealth() == State->HealthMark, TEXT("No damage at the start of the swing"));
				NextStep();
			}
			else if (Elapsed() > 2.0f)
			{
				Check(false, TEXT("Auto-attack started a swing"));
				NextStep();
			}
			break;

		case 5: // damage on the animation's hit frame
			if (WolfHealth->GetCurrentHealth() < State->HealthMark)
			{
				const double Delay = Now() - State->Mark;
				Check(Delay >= 0.3, FString::Printf(TEXT("Swing damage lands on the hit frame (%.2fs into the swing)"), Delay));
				Check(Combat->WasLastHitFromNotify(), TEXT("Hit timing came from the attack animation's MMO Melee Hit notify"));
				Check(FMath::IsNearlyEqual(State->HealthMark - WolfHealth->GetCurrentHealth(), Combat->BasicAttackDamage), TEXT("Swing deals Basic Attack damage"));
				State->Mark = Now();
				State->HealthMark = WolfHealth->GetCurrentHealth();
				Shot(TEXT("1_AutoAttack"));
				NextStep();
			}
			else if (Elapsed() > 2.0f)
			{
				Check(false, TEXT("Swing dealt damage within 2s"));
				NextStep();
			}
			break;

		case 6: // second swing happens on its own on the swing timer
			if (WolfHealth->GetCurrentHealth() < State->HealthMark)
			{
				const double Interval = Now() - State->Mark;
				Check(FMath::Abs(Interval - Combat->BasicAttackCooldown) < 0.3, FString::Printf(TEXT("Auto-attack swings again automatically on the swing timer (%.2fs)"), Interval));
				NextStep();
			}
			else if (Elapsed() > Combat->BasicAttackCooldown + 2.0f)
			{
				Check(false, TEXT("Auto-attack swung again without input"));
				NextStep();
			}
			break;

		case 7: // step out of range (wolf held in place for the test)
			if (!Combat->IsSwingPending())
			{
				// hold the wolf in place (AI state changes reset its speed, so disable movement instead)
				Wolf->GetCharacterMovement()->DisableMovement();
				State->CounterMark = Combat->GetOutOfRangeWarningCount();
				State->HealthMark = WolfHealth->GetCurrentHealth();
				State->bFlag = false;
				PlacePlayerNear(Wolf->GetActorLocation(), 500.0f);
				NextStep();
			}
			break;

		case 8:
			State->bFlag |= WolfHealth->GetCurrentHealth() != State->HealthMark;
			if (Elapsed() > 3.0f)
			{
				const int32 Warnings = Combat->GetOutOfRangeWarningCount() - State->CounterMark;
				Check(!State->bFlag && !Combat->IsTargetInRange(), FString::Printf(TEXT("No damage while out of range (gap %.0fcm)"), UMMOCombatComponent::GetEdgeDistance(Player, Wolf)));
				Check(Combat->IsAutoAttacking(), TEXT("Auto-attack stays active while out of range"));
				Check(Warnings >= 1 && Warnings <= 2, FString::Printf(TEXT("\"Out of Range\" feedback shown and throttled (%d in 3s)"), Warnings));
				PlacePlayerNear(Wolf->GetActorLocation(), 120.0f);
				NextStep();
			}
			break;

		case 9: // back in range: resumes automatically
			if (WolfHealth->GetCurrentHealth() < State->HealthMark)
			{
				Check(Elapsed() < Combat->BasicAttackCooldown + 1.0f, TEXT("Auto-attack resumes automatically back in range"));
				Wolf->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				NextStep();
			}
			else if (Elapsed() > Combat->BasicAttackCooldown + 2.0f)
			{
				Check(false, TEXT("Auto-attack resumed back in range"));
				Wolf->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				NextStep();
			}
			break;

		case 10: // fight to the death on auto-attack
			if (WolfHealth->IsDead())
			{
				Check(!Combat->IsAutoAttacking(), TEXT("Auto-attack stops when the target dies"));
				Check(Wolf->IsDead() && !Wolf->IsTargetable(), TEXT("Dead wolf is not targetable"));
				Check(StateOf(Wolf) == EMMOCreatureAIState::Dead && !Wolf->IsAttacking(), TEXT("Dead wolf AI and attacks stop"));
				Check(Progression->GetCurrentXP() == Wolf->XPReward, TEXT("Killing the wolf awards its XP"));
				Check(Combat->StartAutoAttack() == EMMOAttackResult::TargetDead, TEXT("Cannot attack a dead target"));
				WolfHealth->ApplyDamage(10.0f, Player);
				Check(Progression->GetCurrentXP() == Wolf->XPReward, TEXT("XP is awarded exactly once"));
				PlacePlayerNear(WolfHome, 1500.0f);
				NextStep();
			}
			else if (Elapsed() > 25.0f)
			{
				Check(false, TEXT("Wolf killed by auto-attack within 25s"));
				NextStep();
			}
			break;

		case 11: // respawn
			if (!Wolf->IsDead())
			{
				Check(Elapsed() >= Wolf->RespawnDelay - 0.5f, TEXT("Wolf respawns after its respawn delay"));
				Check(WolfHealth->GetCurrentHealth() == WolfHealth->GetMaxHealth() && Wolf->IsTargetable(), TEXT("Respawned wolf has full health and is targetable"));
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

		case 12: // XP overflow + level up
			Progression->AddXP(90);
			Check(Progression->GetLevel() == 2 && Progression->GetCurrentXP() == Wolf->XPReward + 90 - 100 && Progression->GetXPToNextLevel() == 150, TEXT("Level up at 100 XP with overflow; level 2 needs 150"));
			Check(FMath::IsNearlyEqual(PlayerHealth->GetMaxHealth(), 110.0f) && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 110.0f), TEXT("Level up raises max health and heals"));
			Check(FMath::IsNearlyEqual(Combat->BasicAttackDamage, 14.0f), TEXT("Level up raises Basic Attack damage"));
			NextStep();
			break;

		case 13:
			if (Elapsed() > 0.4f)
			{
				Shot(TEXT("2_LevelUp"));
				Combat->SetTarget(Wolf);
				Check(Combat->StartAutoAttack() == EMMOAttackResult::OutOfRange && Combat->IsAutoAttacking(), TEXT("Starting auto-attack out of range warns but stays armed"));
				Combat->StopAutoAttack();
				Check(!Combat->IsAutoAttacking(), TEXT("Auto-attack toggles off"));
				Check(StateOf(Wolf) == EMMOCreatureAIState::Idle, TEXT("Wolf ignores a player outside aggro range"));
				PlacePlayerNear(WolfHome, 700.0f);
				NextStep();
			}
			break;

		case 14: // leash
			if (InCombat(Wolf))
			{
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

		case 15:
			if (StateOf(Wolf) == EMMOCreatureAIState::Returning)
			{
				Check(true, TEXT("Wolf leashes and returns when pulled too far from spawn"));
				Check(WolfHealth->ApplyDamage(5.0f, Player) == 0.0f, TEXT("Returning wolf evades damage"));
				Wolf->LeashRange = State->OriginalLeashRange;
				Combat->ClearTarget();
				PlacePlayerNear(WolfHome, 1500.0f);
				NextStep();
			}
			else if (Elapsed() > 15.0f)
			{
				Check(false, TEXT("Wolf leashed within 15s"));
				NextStep();
			}
			break;

		case 16:
			if (StateOf(Wolf) == EMMOCreatureAIState::Idle)
			{
				Check(FVector::Dist2D(Wolf->GetActorLocation(), WolfHome) < 100.0f, TEXT("Wolf paths home and resets at its spawn point"));
				Check(WolfHealth->GetCurrentHealth() == WolfHealth->GetMaxHealth() && !WolfHealth->IsInvulnerable(), TEXT("Reset wolf has full health and is attackable"));
				NextStep();
			}
			else if (Elapsed() > 15.0f)
			{
				Check(false, TEXT("Wolf got home within 15s"));
				NextStep();
			}
			break;

		case 17: // several wolves: pull the pair
		{
			AMMOCreature* A = State->Pack[0].Get();
			AMMOCreature* B = State->Pack[1].Get();
			const FVector Middle = (A->GetSpawnTransform().GetLocation() + B->GetSpawnTransform().GetLocation()) * 0.5f;
			PlacePlayerNear(Middle, 450.0f);
			NextStep();
			break;
		}

		case 18:
		{
			AMMOCreature* A = State->Pack[0].Get();
			AMMOCreature* B = State->Pack[1].Get();
			if (InCombat(A) && InCombat(B))
			{
				Check(true, TEXT("Several wolves independently aggro the player"));
				Combat->SetTarget(B);
				Combat->StartAutoAttack();
				State->Mark = Now();
				State->HealthMark = B->GetHealth()->GetCurrentHealth();
				State->OtherHealthMark = A->GetHealth()->GetCurrentHealth();
				NextStep();
			}
			else if (Elapsed() > 5.0f)
			{
				Check(false, TEXT("Both wolves of the pair aggroed"));
				NextStep();
			}
			break;
		}

		case 19:
		{
			AMMOCreature* A = State->Pack[0].Get();
			AMMOCreature* B = State->Pack[1].Get();
			if (Elapsed() > 6.0f)
			{
				Check(B->GetHealth()->GetCurrentHealth() < State->HealthMark, TEXT("Auto-attack damages the selected wolf"));
				Check(A->GetHealth()->GetCurrentHealth() == State->OtherHealthMark, TEXT("Other aggressive wolves are not hit by auto-attack"));
				Check(A->GetLastAttackTime() > State->Mark - 2.0 && B->GetLastAttackTime() > State->Mark - 2.0, TEXT("Both wolves attack the player"));
				Check(Combat->GetCurrentTarget() == B, TEXT("Selected target stays selected while other wolves attack"));
				Shot(TEXT("3_Pack"));
				Player->DoClearTarget();
				Check(Combat->GetCurrentTarget() == nullptr && !Combat->IsAutoAttacking(), TEXT("Clearing the target (Esc) deselects and stops auto-attack"));
				NextStep();
			}
			break;
		}

		case 20: // player death
			PlayerHealth->ApplyDamage(10000.0f, State->Pack[0].Get());
			Check(Player->IsDead() && FMath::IsNearlyEqual(PlayerHealth->GetCurrentHealth(), 0.0f), TEXT("Player dies at zero health (never below)"));
			Check(Combat->StartAutoAttack() == EMMOAttackResult::AttackerDead, TEXT("Dead player cannot attack"));
			State->bFlag = false;
			NextStep();
			break;

		case 21:
			if (!State->bFlag && Elapsed() > 0.6f)
			{
				State->bFlag = true;
				Shot(TEXT("4_Death"));
			}
			if (!Player->IsDead())
			{
				Check(Elapsed() >= Player->GetRespawnDelay() - 0.5f, TEXT("Player respawns after the respawn delay"));
				Check(PlayerHealth->GetCurrentHealth() == PlayerHealth->GetMaxHealth() && Player->InputEnabled(), TEXT("Respawned player has full health and control"));
				Check(Progression->GetLevel() == 2, TEXT("Level is kept through death"));
				Check(!InCombat(State->Pack[0].Get()) && !InCombat(State->Pack[1].Get()), TEXT("Wolves disengage after the player dies"));
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

		// the lone wolf (nearest the player start) is the main test subject; the others form the pack
		TArray<AMMOCreature*> Creatures;
		for (TActorIterator<AMMOCreature> It(World); It; ++It)
		{
			Creatures.Add(*It);
		}
		Creatures.Sort([](const AMMOCreature& A, const AMMOCreature& B)
		{
			return FVector::DistSquared(A.GetActorLocation(), State->SafeOrigin) < FVector::DistSquared(B.GetActorLocation(), State->SafeOrigin);
		});
		if (Creatures.Num() > 0)
		{
			State->Wolf = Creatures[0];
		}
		for (int32 i = 1; i < Creatures.Num(); ++i)
		{
			State->Pack.Add(Creatures[i]);
		}

		UE_LOG(LogMMO, Display, TEXT("MMO SELFTEST START (player: %s, wolf: %s, pack: %d)"), *GetNameSafe(State->Player.Get()), *GetNameSafe(State->Wolf.Get()), State->Pack.Num());

		if (!State->Player.IsValid() || !State->Wolf.IsValid() || State->Pack.Num() < 2)
		{
			Check(false, TEXT("Test world has a player and at least three wolves"));
			Finish();
			return;
		}

		State->StepStart = Now();
		State->Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick), 0.0f);
	}
}

static FAutoConsoleCommandWithWorldAndArgs GMMOSelfTestCommand(
	TEXT("mmo.selftest"),
	TEXT("Runs the combat loop self-test in the current world. Options: 'quit' exits when finished, 'shots' saves screenshots."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&MMOSelfTest::Run));

#endif // !UE_BUILD_SHIPPING

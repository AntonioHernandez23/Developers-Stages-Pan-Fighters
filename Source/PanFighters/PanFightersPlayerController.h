// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/PanFightersMenuTypes.h"
#include "PanFightersPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class SPanFightersMasterWidget;
class ACameraActor;
class UAudioComponent;
class USoundBase;
class APanFightersDropBeacon;

/**
 *  Pan Fighters Player Controller
 *  Manages input mapping, startup 15-second splash screen,
 *  main menu (Jugar, Opciones, Salir), stage selection, first-person gameplay,
 *  pause system with 40% blur/dimming, and stage transitions.
 */
UCLASS(abstract, config="Game")
class PANFIGHTERS_API APanFightersPlayerController : public APlayerController
{
	GENERATED_BODY()
	friend class FGalloGatoMechanicsTest;
	
public:

	/** Constructor */
	APanFightersPlayerController();

	/** Selects a character and transitions to stage selection */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Character")
	void SelectCharacter(EPanFightersCharacter InCharacter);

	/** Sets active character preview in selection screen without leaving the menu */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Character")
	void SetPreviewCharacter(EPanFightersCharacter InCharacter);

	/** Confirms the currently selected character and advances to stage selection */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Character")
	void ConfirmCharacterAndProceed();

	/** Rotates the 3D preview rooster actor by yaw delta */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Character")
	void RotatePreviewRooster(float DeltaYaw);

	/** Updates preview rooster gaze / hover towards mouse position */
	void UpdatePreviewRoosterHover(FVector2D ScreenPos);

	/** Finds or gets the 3D preview rooster actor */
	AActor* GetOrCreatePreviewRooster();

	/** Updates the visual skin/materials of the preview rooster */
	void UpdatePreviewRoosterVisuals();

	/** Returns the currently selected character */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Character")
	EPanFightersCharacter GetSelectedCharacter() const { return SelectedCharacter; }

	/** Returns to the character selection menu */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ReturnToCharacterSelect();

	/** Displays the match result screen (Victory or Defeat) with Retry, Main Menu, and Quit options */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Gameplay")
	void ShowMatchResult(bool bVictory);

	/** Retries the current match/stage */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Gameplay")
	void RetryMatch();

	/** Test Victory screen trigger (Debug hotkey V) */
	void TestVictoryAction();

	/** Test Defeat screen trigger (Debug hotkey B) */
	void TestDefeatAction();

	/** Test Hit Reaction screen flash trigger (Debug hotkey H) */
	void TestHitReactionAction();

	/** Test Damage to player trigger (Debug hotkey K) */
	void TestDamagePlayerAction();

	/** Test Damage to rival trigger (Debug hotkey J) */
	void TestDamageRivalAction();

	/** Returns whether the last match result was victory for player */
	UFUNCTION(BlueprintPure, Category = "PanFighters|UI")
	bool IsLastMatchVictory() const { return bLastMatchVictory; }

	/** Returns current round number (1..3) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Gameplay")
	int32 GetCurrentRound() const { return CurrentRound; }

	/** Returns rounds won by the player (0..2) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Gameplay")
	int32 GetPlayerRoundWins() const { return PlayerRoundWins; }

	/** Returns rounds won by the rival (0..2) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Gameplay")
	int32 GetRivalRoundWins() const { return RivalRoundWins; }

	/** Returns whether the current match ended after playing rounds (vs instant debug hotkey) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Gameplay")
	bool WasMatchCompletedByRounds() const { return bMatchCompletedNormally; }

	/** Returns display name of the winning rooster for the MatchResult showcase */
	UFUNCTION(BlueprintPure, Category = "PanFighters|UI")
	FText GetWinnerCharacterName() const;

	/** Returns current player health (0 to 100) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Combat")
	float GetPlayerCurrentHealth() const { return CurrentPlayerHealth; }

	/** Returns current rival health (0 to 100) */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Combat")
	float GetRivalCurrentHealth() const { return CurrentRivalHealth; }

	/** Returns display name of active rival */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Combat")
	FText GetRivalCharacterName() const;

	/** Triggers screen reaction (red hit vignette and camera punch impulse) when player receives a hit */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Combat")
	void TriggerPlayerHitReaction(float DamageAmount = 25.0f);

	/** Applies damage directly to player (used for debug / testing) */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Combat")
	void ApplyDamageToPlayer(float DamageAmount);

	/** Applies damage directly to active rival (used for debug / testing) */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Combat")
	void ApplyDamageToRival(float DamageAmount);

	/** Finds the active rival rooster in the current level */
	AActor* GetActiveRival() const;

	/** Helper to read Health property from actor via reflection */
	float GetActorHealth(AActor* InActor) const;

	/** Helper to write Health property to actor via reflection */
	void SetActorHealth(AActor* InActor, float NewHealth);

	/** Per-frame player tick */
	virtual void PlayerTick(float DeltaTime) override;

	/** Switch to a specific stage in the game in first person view */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Stages")
	void SelectStage(EPanFightersStage Stage);

	/** Switch the current screen state */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void SetScreenState(EPanFightersScreenState NewState);

	/** Toggles the in-game pause menu (ESC or P) */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ToggleInGameMenu();

	/** Resumes the game from pause */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ResumeGame();

	/** Returns to the main menu */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ReturnToMainMenu();

	/** Returns to the stage selection menu */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void ReturnToStageSelect();

	/** Quits the game */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|UI")
	void QuitGameAction();

	/** Called when master widget changes screen state */
	void OnScreenStateChanged(EPanFightersScreenState NewState);

	/** Returns current active stage */
	UFUNCTION(BlueprintPure, Category = "PanFighters|Stages")
	EPanFightersStage GetCurrentStage() const { return CurrentStage; }

	/** Returns current screen state */
	UFUNCTION(BlueprintPure, Category = "PanFighters|UI")
	EPanFightersScreenState GetCurrentScreenState() const { return CurrentScreenState; }

	/** Startup splash loading bar duration in seconds (5 seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PanFighters|UI")
	float SplashDuration = 5.0f;

	/** Whether startup splash is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PanFighters|UI")
	bool bEnableStartupSplash = true;

	/** Plays or crossfades background music */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Audio")
	void PlayBGM(USoundBase* NewMusic, float FadeTime = 0.8f);

	/** Stops background music with fade */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Audio")
	void StopBGM(float FadeTime = 0.8f);

	/** Plays a 2D sound effect with volume and pitch modulation */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Audio")
	void PlaySFX2D(USoundBase* SoundToPlay, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	/** Plays a 3D sound effect at a world location */
	UFUNCTION(BlueprintCallable, Category = "PanFighters|Audio")
	void PlaySFXAtLocation(USoundBase* SoundToPlay, FVector Location, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

protected:

	/** Audio component for background music */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BGMComponent;

	/** Music track for Splash & Main Menu */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Audio")
	TObjectPtr<USoundBase> MainMenuMusic;

	/** Music track for Character Selection & Stage Selection */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Audio")
	TObjectPtr<USoundBase> SelectionMenuMusic;

	/** Music track for In-Game Rooster Fight */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Audio")
	TObjectPtr<USoundBase> FightMusic;

	/** Music track for match victory */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Audio")
	TObjectPtr<USoundBase> VictoryMusic;

	/** Boxing bell sound effect (round start and pause) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Audio")
	TObjectPtr<USoundBase> BoxingBellSound;

	/** Punch hit sound effect (when attack connects) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Audio")
	TObjectPtr<USoundBase> PunchHitSound;

	/** Rooster hurt sound effect (emitted by player or rival on hit) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Audio")
	TObjectPtr<USoundBase> RoosterHurtSound;

	/** Dedicated close-up camera for character selection with Depth of Field */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> CharacterSelectCameraActor;

	/** Circular flat ring canvas mat actor at rooster's feet */
	UPROPERTY(Transient)
	TObjectPtr<AActor> RingCanvasPedestalActor;

	/** Saved hidden powerup actors during character selection */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> HiddenPowerupActors;

	/** Sets up the close-up camera, ring mat, and 3D rooster (Character Select or Winner Showcase) */
	void SetupRoosterShowcaseScene(bool bIsVictoryShowcase, bool bPlayerWon);

	/** Cleans up the character select / showcase scene props and camera */
	void CleanupCharacterSelectScene();

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Currently selected playable character */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Character")
	EPanFightersCharacter SelectedCharacter = EPanFightersCharacter::PolloBlanco;

	/** Reference to the 3D preview rooster actor during character selection */
	UPROPERTY(Transient)
	TObjectPtr<AActor> PreviewRoosterActor;

	/** Base yaw orientation of preview rooster */
	float PreviewRoosterBaseYaw = 0.0f;

	/** Ground location of the character select ring canvas mat for adaptive foot placement */
	UPROPERTY(Transient)
	FVector PreviewMatLocation = FVector::ZeroVector;

	/** Whether the last match result was a victory */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Gameplay")
	bool bLastMatchVictory = true;

	/** Current monitored player health (0 to 100 visual percentage) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Combat")
	float CurrentPlayerHealth = 100.0f;

	/** Previous frame player health (0 to 100) for hit detection */
	float PreviousPlayerHealth = 100.0f;

	/** Current monitored rival health (0 to 100 visual percentage) */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Combat")
	float CurrentRivalHealth = 100.0f;

	/** Previous frame rival health (0 to 100) for hit detection */
	float PreviousRivalHealth = 100.0f;

	/** Internal maximum health for player (visual bars remain 0 to 100) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Combat")
	float MaxPlayerHealth = 350.0f;

	/** Internal maximum health for rivals (visual bars remain 0 to 100) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Combat")
	float MaxRivalHealth = 350.0f;

	/** Baseline walk speed restored whenever a stage is selected or buffs expire */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Combat")
	float DefaultPlayerWalkSpeed = 300.0f;

	/** Baseline movement speed enforced on all rival roosters (+5% over original 245) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PanFighters|Combat")
	float DefaultRivalWalkSpeed = 257.25f;

	/** Active sky-drop beacons spawned for the current stage */
	TArray<TWeakObjectPtr<APanFightersDropBeacon>> ActiveDropBeacons;

	/** Timer used to replenish sky-drop items mid-match */
	float SkyDropRespawnTimer = 0.0f;

	/** Randomized delay (seconds) before spawning a replacement sky drop when below the arena cap */
	float NextSkyDropRespawnInterval = 5.0f;

	/** Cached initial transforms for each stage's rival across rounds and retries */
	TMap<EPanFightersStage, FTransform> CachedStageRivalSpawns;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedRivalBodyMat;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CachedRivalChestMat;

	/** Best-of-3 match round tracking (first rooster to win 2 rounds wins the match) */
	static constexpr int32 RoundsToWinMatch = 2;
	static constexpr int32 MaxMatchRounds = 3;

	int32 CurrentRound = 1;
	int32 PlayerRoundWins = 0;
	int32 RivalRoundWins = 0;
	bool bWaitingNextRound = false;
	float RoundTransitionTimer = 0.0f;
	bool bMatchCompletedNormally = false;

	/** Timer after match/round decisive hit before round transition or result screen */
	float MatchFinishTimer = 0.0f;

	/** Flag to prevent multiple win/loss triggers */
	bool bMatchResolved = false;

	/** Initializes/resets fighters and items to begin the current round */
	void StartRound(bool bInitialStageEntry);

	/** Resolves a round knockout (advances to next round or finishes Best-of-3 match) */
	void HandleRoundEnded(bool bPlayerWonRound);

	/** Snapshots body & chest materials from the stage rival actor for the Winner showcase */
	void CacheRivalAppearance(AActor* RivalActor);

	/** Applies selected character's SkeletalMesh, mesh pivot offset, and materials (handles GalloGato + Pollo skins) */
	void ApplyRoosterAppearance(AActor* RoosterActor, EPanFightersCharacter InCharacter);

	/** Synchronizes custom character animations & floor pivot with BP_GalloBlanco combat/showcase state */
	void SyncGalloGatoCombatAnimation(AActor* TargetActor, bool bForceVictory = false);

	/** Tracks last synced combat state so looping/one-shot animations transition reliably without restarting */
	TWeakObjectPtr<AActor> LastSyncedGalloGatoActor;
	int32 LastGalloGatoAnimState = -1;
	float LastGalloGatoStateTimer = 0.0f;
	int32 GalloGatoJabCounter = 0;

	/** Camera punch recoil trauma */
	float PunchTraumaPitch = 0.0f;
	float PunchTraumaRoll = 0.0f;

	/** Currently active stage */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|Stages")
	EPanFightersStage CurrentStage = EPanFightersStage::None;

	/** Currently active screen state */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "PanFighters|UI")
	EPanFightersScreenState CurrentScreenState = EPanFightersScreenState::None;

	/** Slate master UI instance */
	TSharedPtr<SPanFightersMasterWidget> MasterMenuWidget;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Returns the normalized 0..100 health percentage for UI display */
	float GetActorNormalizedHealth(AActor* InActor, float MaxHealth) const;

	/** Resets player and rival Blueprint/character states, speeds, effects, and health */
	void ResetCharacterAndRivalStates();

	/** Clears all active sky drops and leftover PowerUp actors in the world */
	void ClearStagePowerUps();

	/** Regenerates sky-falling PowerUps strictly within the target stage's playable platform */
	void RegenerateStagePowerUps(EPanFightersStage Stage);

	/** Prunes collected sky drops and spawns replacement items continuously during active combat */
	void TryReplenishStagePowerUp(EPanFightersStage Stage, float DeltaTime);

	/** Collects locations to avoid when picking a landing point (player, rival, and active beacons) */
	TArray<FVector> BuildPowerUpAvoidPoints(EPanFightersStage Stage) const;

	/** Spawns one PowerUp from the sky with a visual landing indicator on the target platform */
	void SpawnSkyDropItem(EPanFightersStage Stage, AActor* PlatformActor, UClass* PowerUpClass, const FLinearColor& BeaconColor, TArray<FVector>& InOutUsedPoints, float SpawnDelaySeconds = 0.0f);

	/** Finds the playable platform actor for the given stage */
	AActor* FindStagePlatformActor(EPanFightersStage Stage) const;

	/** Finds a validated random landing point strictly on the stage playable platform */
	bool FindRandomDropPointOnPlatform(EPanFightersStage Stage, AActor* PlatformActor, const TArray<FVector>& AvoidLocations, FVector& OutFloorPoint) const;

	/** Stage keyword and fallback arena center helpers */
	static FString GetStageKeyword(EPanFightersStage Stage);
	static FVector GetStageFallbackCenter(EPanFightersStage Stage);

	/** Finds the CameraActor for a given stage in the active world */
	ACameraActor* FindStageCamera(EPanFightersStage Stage) const;

	/** Finds the PlayerStart actor for a given stage in the active world */
	AActor* FindStagePlayerStart(EPanFightersStage Stage) const;
};

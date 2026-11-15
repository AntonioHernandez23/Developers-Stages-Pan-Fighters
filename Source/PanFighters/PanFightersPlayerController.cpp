// Copyright Epic Games, Inc. All Rights Reserved.

#include "PanFightersPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "PanFightersCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "PanFighters.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "UI/SPanFightersMasterWidget.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "ImageUtils.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UObject/UObjectIterator.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "UObject/ConstructorHelpers.h"

APanFightersPlayerController::APanFightersPlayerController()
{
	PlayerCameraManagerClass = APanFightersCameraManager::StaticClass();
	SplashDuration = 5.0f;
	bEnableStartupSplash = true;

	static ConstructorHelpers::FObjectFinder<USoundBase> MainMenuMusicFinder(TEXT("/Game/Audio/BGM/Mus_MainMenu.Mus_MainMenu"));
	if (MainMenuMusicFinder.Succeeded())
	{
		MainMenuMusic = MainMenuMusicFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> SelectionMusicFinder(TEXT("/Game/Audio/BGM/Mus_Selection.Mus_Selection"));
	if (SelectionMusicFinder.Succeeded())
	{
		SelectionMenuMusic = SelectionMusicFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> FightMusicFinder(TEXT("/Game/Audio/BGM/Mus_Fight.Mus_Fight"));
	if (FightMusicFinder.Succeeded())
	{
		FightMusic = FightMusicFinder.Object;
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<USoundBase> FightFallbackFinder(TEXT("/Game/Audio/BGM/kf029904-at-war.kf029904-at-war"));
		if (FightFallbackFinder.Succeeded())
		{
			FightMusic = FightFallbackFinder.Object;
		}
	}
}

void APanFightersPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Initialize master UI (Splash screen -> Main Menu -> Stage Selection -> InGame / Pause)
	if (IsLocalPlayerController() && GEngine && GEngine->GameViewport)
	{
		MasterMenuWidget = SNew(SPanFightersMasterWidget)
			.OwnerPlayerController(this)
			.SplashDuration(SplashDuration);

		GEngine->GameViewport->AddViewportWidgetContent(MasterMenuWidget.ToSharedRef(), 100);

		if (bEnableStartupSplash)
		{
			SetScreenState(EPanFightersScreenState::Splash);
		}
		else
		{
			SetScreenState(EPanFightersScreenState::MainMenu);
		}
	}

	// Only spawn touch controls on local player controllers if needed
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);
		if (MobileControlsWidget)
		{
			MobileControlsWidget->AddToPlayerScreen(0);
		}
		else
		{
			UE_LOG(LogPanFighters, Error, TEXT("Could not spawn mobile controls widget."));
		}
	}
}

void APanFightersPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (IsLocalPlayerController())
	{
		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}

		// Bind standard keys for Pause and Menu toggle (ESC, P, M) with bExecuteWhenPaused = true
		if (InputComponent)
		{
			FInputKeyBinding& EscBinding = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APanFightersPlayerController::ToggleInGameMenu);
			EscBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& PBinding = InputComponent->BindKey(EKeys::P, IE_Pressed, this, &APanFightersPlayerController::ToggleInGameMenu);
			PBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& MBinding = InputComponent->BindKey(EKeys::M, IE_Pressed, this, &APanFightersPlayerController::ToggleInGameMenu);
			MBinding.bExecuteWhenPaused = true;

			// Debug hotkeys to test Victory / Defeat screens in-game (V = Victory, B = Defeat)
			FInputKeyBinding& VBinding = InputComponent->BindKey(EKeys::V, IE_Pressed, this, &APanFightersPlayerController::TestVictoryAction);
			VBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& BBinding = InputComponent->BindKey(EKeys::B, IE_Pressed, this, &APanFightersPlayerController::TestDefeatAction);
			BBinding.bExecuteWhenPaused = true;

			// Combat reaction and damage test hotkeys (H = Hit reaction, K = Damage player, J = Damage rival)
			FInputKeyBinding& HBinding = InputComponent->BindKey(EKeys::H, IE_Pressed, this, &APanFightersPlayerController::TestHitReactionAction);
			HBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& KBinding = InputComponent->BindKey(EKeys::K, IE_Pressed, this, &APanFightersPlayerController::TestDamagePlayerAction);
			KBinding.bExecuteWhenPaused = true;

			FInputKeyBinding& JBinding = InputComponent->BindKey(EKeys::J, IE_Pressed, this, &APanFightersPlayerController::TestDamageRivalAction);
			JBinding.bExecuteWhenPaused = true;
		}
	}
}

void APanFightersPlayerController::SetScreenState(EPanFightersScreenState NewState)
{
	if (MasterMenuWidget.IsValid())
	{
		MasterMenuWidget->SetScreenState(NewState);
	}
}

void APanFightersPlayerController::PlayBGM(USoundBase* NewMusic, float FadeTime)
{
	if (!NewMusic)
	{
		return;
	}

	if (USoundWave* Wave = Cast<USoundWave>(NewMusic))
	{
		Wave->bLooping = true;
	}

	if (BGMComponent && BGMComponent->Sound == NewMusic && BGMComponent->IsPlaying())
	{
		return;
	}

	if (BGMComponent && BGMComponent->IsPlaying())
	{
		BGMComponent->FadeOut(FadeTime, 0.0f);
	}

	BGMComponent = UGameplayStatics::SpawnSound2D(this, NewMusic, 1.0f, 1.0f, 0.0f, nullptr, true, true);
	if (BGMComponent)
	{
		BGMComponent->bIsUISound = true;
		BGMComponent->FadeIn(FadeTime, 1.0f);
	}
}

void APanFightersPlayerController::StopBGM(float FadeTime)
{
	if (BGMComponent && BGMComponent->IsPlaying())
	{
		BGMComponent->FadeOut(FadeTime, 0.0f);
	}
}

void APanFightersPlayerController::OnScreenStateChanged(EPanFightersScreenState NewState)
{
	const EPanFightersScreenState PreviousState = CurrentScreenState;
	CurrentScreenState = NewState;

	if (PreviousState == EPanFightersScreenState::CharacterSelect && NewState != EPanFightersScreenState::CharacterSelect)
	{
		CleanupCharacterSelectScene();
	}

	if (NewState == EPanFightersScreenState::Splash)
	{
		StopBGM(0.0f);
	}
	else if (NewState == EPanFightersScreenState::MainMenu)
	{
		PlayBGM(MainMenuMusic, 0.8f);
	}
	else if (NewState == EPanFightersScreenState::CharacterSelect)
	{
		PlayBGM(SelectionMenuMusic, 0.8f);

		GetOrCreatePreviewRooster();
		UpdatePreviewRoosterVisuals();
		SetupCharacterSelectScene();
	}
	else if (NewState == EPanFightersScreenState::StageSelect)
	{
		PlayBGM(SelectionMenuMusic, 0.8f);
	}
	else if (NewState == EPanFightersScreenState::InGame)
	{
		if (!FightMusic)
		{
			FightMusic = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/BGM/Mus_Fight.Mus_Fight")));
			if (!FightMusic)
			{
				FightMusic = Cast<USoundBase>(StaticLoadObject(USoundBase::StaticClass(), nullptr, TEXT("/Game/Audio/BGM/kf029904-at-war.kf029904-at-war")));
			}
		}

		if (FightMusic)
		{
			PlayBGM(FightMusic, 0.8f);
		}
		else
		{
			StopBGM(1.0f);
		}
	}
	else if (NewState == EPanFightersScreenState::MatchResult)
	{
		StopBGM(1.2f);
	}

	if (NewState == EPanFightersScreenState::InGame)
	{
		// Unpause world physics and gameplay
		UGameplayStatics::SetGamePaused(GetWorld(), false);

		// First-Person gameplay input mode: capture mouse, hide cursor
		FInputModeGameOnly GameInputMode;
		SetInputMode(GameInputMode);
		bShowMouseCursor = false;
	}
	else if (NewState == EPanFightersScreenState::InGameMenu || NewState == EPanFightersScreenState::MatchResult)
	{
		// Pause the game world physics and actors
		UGameplayStatics::SetGamePaused(GetWorld(), true);

		// Pause & Match Result menu interactive UI mode: show mouse cursor for clicking buttons
		FInputModeGameAndUI PauseInputMode;
		PauseInputMode.SetHideCursorDuringCapture(false);
		PauseInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(PauseInputMode);
		bShowMouseCursor = true;
	}
	else
	{
		// Main Menu, Options, Stage Select, Splash
		UGameplayStatics::SetGamePaused(GetWorld(), false);

		FInputModeUIOnly UIInputMode;
		UIInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(UIInputMode);
		bShowMouseCursor = true;
	}
}

void APanFightersPlayerController::SelectStage(EPanFightersStage Stage)
{
	CleanupCharacterSelectScene();

	CurrentStage = Stage;

	// Reset combat states for the new / retried match
	bMatchResolved = false;
	MatchFinishTimer = 0.0f;
	CurrentPlayerHealth = 100.0f;
	PreviousPlayerHealth = 100.0f;
	CurrentRivalHealth = 100.0f;

	// Suppress any legacy WBP_ArenaHUD widget
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* W = *It;
		if (W && W->GetWorld() == GetWorld() && W->IsInViewport())
		{
			if (W->GetClass()->GetName().Contains(TEXT("WBP_ArenaHUD")))
			{
				W->SetVisibility(ESlateVisibility::Collapsed);
				W->RemoveFromParent();
			}
		}
	}

	FText StageNameText;
	FText StageSubtitleText;

	switch (Stage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		StageNameText = FText::FromString(TEXT("El Ranchito"));
		StageSubtitleText = FText::FromString(TEXT("Bohío Campesino Tradicional y Ruedo de las Aves"));
		break;

	case EPanFightersStage::Stage2_ElCampoLlano:
		StageNameText = FText::FromString(TEXT("El Campo Llano"));
		StageSubtitleText = FText::FromString(TEXT("La Sabana y Potrero a Campo Abierto"));
		break;

	case EPanFightersStage::Stage3_ElSarten:
		StageNameText = FText::FromString(TEXT("El Sartén"));
		StageSubtitleText = FText::FromString(TEXT("La Cocina Campesina al Fuego Vivo"));
		break;

	default:
		break;
	}

	// 1. Ensure world is unpaused
	UGameplayStatics::SetGamePaused(GetWorld(), false);

	// 2. Find stage references (Camera and/or PlayerStart)
	ACameraActor* StageCam = FindStageCamera(Stage);
	AActor* PlayerStartActor = FindStagePlayerStart(Stage);

	// 3. Teleport character pawn to stage location in FIRST PERSON view
	APawn* MyPawn = GetPawn();
	if (MyPawn)
	{
		FVector TargetLocation = MyPawn->GetActorLocation();
		FRotator TargetRotation = MyPawn->GetActorRotation();

		if (PlayerStartActor)
		{
			TargetLocation = PlayerStartActor->GetActorLocation();
			TargetRotation = PlayerStartActor->GetActorRotation();
		}
		else if (StageCam)
		{
			TargetLocation = StageCam->GetActorLocation();
			TargetRotation = StageCam->GetActorRotation();

			// Ground trace so character stands securely on stage floor
			if (UWorld* World = GetWorld())
			{
				FHitResult GroundHit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(StageGroundTrace), false, MyPawn);
				if (World->LineTraceSingleByChannel(GroundHit, TargetLocation, TargetLocation - FVector(0.0f, 0.0f, 2500.0f), ECC_Visibility, Params))
				{
					TargetLocation.Z = GroundHit.Location.Z + 100.0f;
				}
				// Advance slightly in front of the camera to avoid any clipping
				TargetLocation += TargetRotation.Vector() * 50.0f;
			}
		}

		TargetRotation.Pitch = 0.0f;
		TargetRotation.Roll = 0.0f;

		MyPawn->TeleportTo(TargetLocation, TargetRotation, false, true);

		// Orient the first-person controller rotation towards the stage
		SetControlRotation(TargetRotation);

		// Crucial: Set view target to the player's own pawn for true First-Person perspective!
		SetViewTarget(MyPawn);

		// Restore full health to player and rival
		SetActorHealth(MyPawn, 100.0f);
		if (AActor* ActiveRival = GetActiveRival())
		{
			SetActorHealth(ActiveRival, 100.0f);
		}

		// Apply selected character skin to player's pawn mesh in combat
		USkeletalMeshComponent* MeshComp = MyPawn->FindComponentByClass<USkeletalMeshComponent>();
		if (MeshComp)
		{
			if (SelectedCharacter == EPanFightersCharacter::PolloChocolate)
			{
				UMaterialInterface* ChocMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/ElRanchito/Materials/MI_PolloCafe.MI_PolloCafe")));
				if (!ChocMat)
				{
					ChocMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Stages/01_ElRanchito/ElRanchito_Assets/Materials/MI_PolloCafe.MI_PolloCafe")));
				}
				if (ChocMat)
				{
					MeshComp->SetMaterial(0, ChocMat);
				}
			}
			else
			{
				UMaterialInterface* WhiteMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Textures/Material_001.Material_001")));
				if (WhiteMat)
				{
					MeshComp->SetMaterial(0, WhiteMat);
				}
			}
		}
	}

	// 4. Notify widget and switch to InGame view
	if (MasterMenuWidget.IsValid())
	{
		MasterMenuWidget->NotifyStageEntered(Stage, StageNameText, StageSubtitleText);
		MasterMenuWidget->SetScreenState(EPanFightersScreenState::InGame);
	}

	// 5. Set input mode to first-person gameplay
	FInputModeGameOnly GameInputMode;
	SetInputMode(GameInputMode);
	bShowMouseCursor = false;
}

void APanFightersPlayerController::ToggleInGameMenu()
{
	if (CurrentScreenState == EPanFightersScreenState::InGame)
	{
		SetScreenState(EPanFightersScreenState::InGameMenu);
	}
	else if (CurrentScreenState == EPanFightersScreenState::InGameMenu)
	{
		ResumeGame();
	}
}

void APanFightersPlayerController::ResumeGame()
{
	SetScreenState(EPanFightersScreenState::InGame);
}

void APanFightersPlayerController::ReturnToMainMenu()
{
	SetScreenState(EPanFightersScreenState::MainMenu);
}

void APanFightersPlayerController::ReturnToStageSelect()
{
	SetScreenState(EPanFightersScreenState::StageSelect);
}

void APanFightersPlayerController::QuitGameAction()
{
	UKismetSystemLibrary::QuitGame(GetWorld(), this, EQuitPreference::Quit, false);
}

void APanFightersPlayerController::SelectCharacter(EPanFightersCharacter InCharacter)
{
	SelectedCharacter = InCharacter;
	UpdatePreviewRoosterVisuals();
	SetScreenState(EPanFightersScreenState::StageSelect);
}

void APanFightersPlayerController::SetPreviewCharacter(EPanFightersCharacter InCharacter)
{
	SelectedCharacter = InCharacter;
	UpdatePreviewRoosterVisuals();
}

void APanFightersPlayerController::ConfirmCharacterAndProceed()
{
	SetScreenState(EPanFightersScreenState::StageSelect);
}

AActor* APanFightersPlayerController::GetOrCreatePreviewRooster()
{
	if (PreviewRoosterActor && IsValid(PreviewRoosterActor))
	{
		return PreviewRoosterActor;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->GetName().Contains(TEXT("Ranchito"), ESearchCase::IgnoreCase) && A->GetName().Contains(TEXT("Gallo"), ESearchCase::IgnoreCase))
		{
			PreviewRoosterActor = A;
			PreviewRoosterBaseYaw = A->GetActorRotation().Yaw;
			return PreviewRoosterActor;
		}
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (A && A->GetClass()->GetName().Contains(TEXT("BP_Gallo")))
		{
			PreviewRoosterActor = A;
			PreviewRoosterBaseYaw = A->GetActorRotation().Yaw;
			return PreviewRoosterActor;
		}
	}

	if (APawn* MyPawn = GetPawn())
	{
		PreviewRoosterActor = MyPawn;
		PreviewRoosterBaseYaw = MyPawn->GetActorRotation().Yaw;
		return PreviewRoosterActor;
	}

	return nullptr;
}

void APanFightersPlayerController::UpdatePreviewRoosterVisuals()
{
	AActor* Rooster = GetOrCreatePreviewRooster();
	if (!Rooster)
	{
		return;
	}

	USkeletalMeshComponent* MeshComp = Rooster->FindComponentByClass<USkeletalMeshComponent>();
	if (!MeshComp)
	{
		return;
	}

	if (SelectedCharacter == EPanFightersCharacter::PolloChocolate)
	{
		UMaterialInterface* ChocMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/ElRanchito/Materials/MI_PolloCafe.MI_PolloCafe")));
		if (!ChocMat)
		{
			ChocMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Stages/01_ElRanchito/ElRanchito_Assets/Materials/MI_PolloCafe.MI_PolloCafe")));
		}

		if (ChocMat)
		{
			MeshComp->SetMaterial(0, ChocMat);
		}
		else
		{
			UMaterialInstanceDynamic* DynMat = MeshComp->CreateDynamicMaterialInstance(0);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("Base Color"), FLinearColor(0.25f, 0.12f, 0.05f, 1.0f));
			}
		}
	}
	else
	{
		UMaterialInterface* WhiteMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Textures/Material_001.Material_001")));
		if (WhiteMat)
		{
			MeshComp->SetMaterial(0, WhiteMat);
		}
	}
}

void APanFightersPlayerController::RotatePreviewRooster(float DeltaYaw)
{
	AActor* Rooster = GetOrCreatePreviewRooster();
	if (Rooster)
	{
		Rooster->AddActorWorldRotation(FRotator(0.0f, DeltaYaw, 0.0f));
	}
}

void APanFightersPlayerController::UpdatePreviewRoosterHover(FVector2D ScreenPos)
{
	AActor* Rooster = GetOrCreatePreviewRooster();
	if (!Rooster)
	{
		return;
	}

	int32 ViewportSizeX = 1920;
	int32 ViewportSizeY = 1080;
	GetViewportSize(ViewportSizeX, ViewportSizeY);

	if (ViewportSizeX > 0 && GetWorld())
	{
		const float NormalizedX = (ScreenPos.X / (float)ViewportSizeX) - 0.5f;
		const float SubtleYawOffset = NormalizedX * 30.0f;

		FRotator TargetRot = Rooster->GetActorRotation();
		TargetRot.Yaw = FMath::FInterpTo(TargetRot.Yaw, PreviewRoosterBaseYaw + SubtleYawOffset, GetWorld()->GetDeltaSeconds(), 6.0f);
		Rooster->SetActorRotation(TargetRot);
	}
}

void APanFightersPlayerController::ReturnToCharacterSelect()
{
	SetScreenState(EPanFightersScreenState::CharacterSelect);
}

void APanFightersPlayerController::ShowMatchResult(bool bVictory)
{
	bLastMatchVictory = bVictory;
	SetScreenState(EPanFightersScreenState::MatchResult);
}

void APanFightersPlayerController::RetryMatch()
{
	if (CurrentStage != EPanFightersStage::None)
	{
		SelectStage(CurrentStage);
	}
	else
	{
		ReturnToStageSelect();
	}
}

void APanFightersPlayerController::TestVictoryAction()
{
	if (CurrentScreenState == EPanFightersScreenState::InGame || CurrentScreenState == EPanFightersScreenState::InGameMenu)
	{
		ShowMatchResult(true);
	}
}

void APanFightersPlayerController::TestDefeatAction()
{
	if (CurrentScreenState == EPanFightersScreenState::InGame || CurrentScreenState == EPanFightersScreenState::InGameMenu)
	{
		ShowMatchResult(false);
	}
}

void APanFightersPlayerController::TestHitReactionAction()
{
	TriggerPlayerHitReaction(25.0f);
}

void APanFightersPlayerController::TestDamagePlayerAction()
{
	ApplyDamageToPlayer(20.0f);
}

void APanFightersPlayerController::TestDamageRivalAction()
{
	ApplyDamageToRival(25.0f);
}

void APanFightersPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (CurrentScreenState == EPanFightersScreenState::InGame)
	{
		// 1. Suppress any legacy WBP_ArenaHUD in viewport so it doesn't conflict with our UI
		for (TObjectIterator<UUserWidget> It; It; ++It)
		{
			UUserWidget* W = *It;
			if (W && W->GetWorld() == GetWorld() && W->IsInViewport())
			{
				if (W->GetClass()->GetName().Contains(TEXT("WBP_ArenaHUD")))
				{
					W->SetVisibility(ESlateVisibility::Collapsed);
					W->RemoveFromParent();
				}
			}
		}

		// 2. Read player health and detect hit
		APawn* MyPawn = GetPawn();
		if (MyPawn)
		{
			CurrentPlayerHealth = FMath::Clamp(GetActorHealth(MyPawn), 0.0f, 100.0f);
			if (CurrentPlayerHealth < PreviousPlayerHealth)
			{
				float DamageTaken = PreviousPlayerHealth - CurrentPlayerHealth;
				TriggerPlayerHitReaction(DamageTaken);
			}
			PreviousPlayerHealth = CurrentPlayerHealth;
		}

		// 3. Read active rival health
		AActor* ActiveRival = GetActiveRival();
		if (ActiveRival)
		{
			CurrentRivalHealth = FMath::Clamp(GetActorHealth(ActiveRival), 0.0f, 100.0f);
		}

		// 4. Check decisive match condition (knockout)
		if (!bMatchResolved)
		{
			if (CurrentRivalHealth <= 0.0f)
			{
				MatchFinishTimer += DeltaTime;
				if (MatchFinishTimer >= 0.85f)
				{
					bMatchResolved = true;
					ShowMatchResult(true); // Player won!
				}
			}
			else if (CurrentPlayerHealth <= 0.0f)
			{
				MatchFinishTimer += DeltaTime;
				if (MatchFinishTimer >= 0.85f)
				{
					bMatchResolved = true;
					ShowMatchResult(false); // Player lost / NPC rooster won!
				}
			}
			else
			{
				MatchFinishTimer = 0.0f;
			}
		}
	}

	// Smooth camera punch recoil decay
	if (PunchTraumaPitch != 0.0f || PunchTraumaRoll != 0.0f)
	{
		PunchTraumaPitch = FMath::FInterpTo(PunchTraumaPitch, 0.0f, DeltaTime, 14.0f);
		PunchTraumaRoll = FMath::FInterpTo(PunchTraumaRoll, 0.0f, DeltaTime, 14.0f);
	}
}

void APanFightersPlayerController::TriggerPlayerHitReaction(float DamageAmount)
{
	// 1. Notify master widget to flash the crimson screen vignette and shake HUD bar
	if (MasterMenuWidget.IsValid())
	{
		MasterMenuWidget->TriggerHitReaction(DamageAmount);
	}

	// 2. Camera punch impulse / head snap
	PunchTraumaPitch = FMath::RandRange(1.2f, 2.2f);
	PunchTraumaRoll = FMath::RandRange(-2.0f, 2.0f);
	AddPitchInput(PunchTraumaPitch * 0.4f);
	AddRollInput(PunchTraumaRoll * 0.4f);
}

void APanFightersPlayerController::ApplyDamageToPlayer(float DamageAmount)
{
	APawn* MyPawn = GetPawn();
	if (MyPawn)
	{
		float NewHealth = FMath::Clamp(CurrentPlayerHealth - DamageAmount, 0.0f, 100.0f);
		SetActorHealth(MyPawn, NewHealth);
		CurrentPlayerHealth = NewHealth;
		TriggerPlayerHitReaction(DamageAmount);
		PreviousPlayerHealth = CurrentPlayerHealth;
	}
}

void APanFightersPlayerController::ApplyDamageToRival(float DamageAmount)
{
	AActor* ActiveRival = GetActiveRival();
	if (ActiveRival)
	{
		float NewHealth = FMath::Clamp(CurrentRivalHealth - DamageAmount, 0.0f, 100.0f);
		SetActorHealth(ActiveRival, NewHealth);
		CurrentRivalHealth = NewHealth;
	}
}

AActor* APanFightersPlayerController::GetActiveRival() const
{
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && Actor->GetClass()->GetName().Contains(TEXT("GalloRival")))
			{
				return Actor;
			}
		}
	}
	return nullptr;
}

FText APanFightersPlayerController::GetRivalCharacterName() const
{
	return FText::FromString(TEXT("GALLO RIVAL"));
}

float APanFightersPlayerController::GetActorHealth(AActor* InActor) const
{
	if (!InActor) return 0.0f;
	if (FProperty* Prop = InActor->GetClass()->FindPropertyByName(TEXT("Health")))
	{
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			return FloatProp->GetPropertyValue_InContainer(InActor);
		}
		if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			return (float)DoubleProp->GetPropertyValue_InContainer(InActor);
		}
	}
	return 100.0f;
}

void APanFightersPlayerController::SetActorHealth(AActor* InActor, float NewHealth)
{
	if (!InActor) return;
	if (FProperty* Prop = InActor->GetClass()->FindPropertyByName(TEXT("Health")))
	{
		if (FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			FloatProp->SetPropertyValue_InContainer(InActor, NewHealth);
		}
		else if (FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			DoubleProp->SetPropertyValue_InContainer(InActor, (double)NewHealth);
		}
	}
}

ACameraActor* APanFightersPlayerController::FindStageCamera(EPanFightersStage Stage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FString StageKeyword;
	switch (Stage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		StageKeyword = TEXT("Ranchito");
		break;
	case EPanFightersStage::Stage2_ElCampoLlano:
		StageKeyword = TEXT("CampoLlano");
		break;
	case EPanFightersStage::Stage3_ElSarten:
		StageKeyword = TEXT("Sarten");
		break;
	default:
		return nullptr;
	}

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		ACameraActor* Cam = *It;
#if WITH_EDITOR
		if (Cam->GetActorLabel().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Cam;
		}
#endif
		if (Cam->GetName().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Cam;
		}
		for (const FName& Tag : Cam->Tags)
		{
			if (Tag.ToString().Contains(StageKeyword, ESearchCase::IgnoreCase))
			{
				return Cam;
			}
		}
	}

	// Fallback by index
	TArray<ACameraActor*> AllCameras;
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		AllCameras.Add(*It);
	}

	if (AllCameras.Num() > 0)
	{
		int32 Index = (int32)Stage - 1;
		if (AllCameras.IsValidIndex(Index))
		{
			return AllCameras[Index];
		}
		return AllCameras[0];
	}

	return nullptr;
}

AActor* APanFightersPlayerController::FindStagePlayerStart(EPanFightersStage Stage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FString StageKeyword;
	switch (Stage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		StageKeyword = TEXT("Ranchito");
		break;
	case EPanFightersStage::Stage2_ElCampoLlano:
		StageKeyword = TEXT("CampoLlano");
		break;
	case EPanFightersStage::Stage3_ElSarten:
		StageKeyword = TEXT("Sarten");
		break;
	default:
		return nullptr;
	}

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
#if WITH_EDITOR
		if (Start->GetActorLabel().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Start;
		}
#endif
		if (Start->GetName().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Start;
		}
		if (Start->PlayerStartTag.ToString().Contains(StageKeyword, ESearchCase::IgnoreCase))
		{
			return Start;
		}
	}

	return nullptr;
}

bool APanFightersPlayerController::ShouldUseTouchControls() const
{
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void APanFightersPlayerController::SetupCharacterSelectScene()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 1. Hide floating powerups (coffee mug, plantain, chili) that clutter the area in front of the preview rooster
	HiddenPowerupActors.Empty();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;
		const FString Name = A->GetName();
		if (Name.Contains(TEXT("TazaCafe"), ESearchCase::IgnoreCase) ||
			Name.Contains(TEXT("Patacon"), ESearchCase::IgnoreCase) ||
			Name.Contains(TEXT("AjiChombo"), ESearchCase::IgnoreCase) ||
			Name.Contains(TEXT("PowerUp"), ESearchCase::IgnoreCase))
		{
			if (!A->IsHidden())
			{
				A->SetActorHiddenInGame(true);
				A->SetActorEnableCollision(false);
				HiddenPowerupActors.Add(A);
			}
		}
	}

	// 2. Locate or get the preview rooster
	AActor* Rooster = GetOrCreatePreviewRooster();
	if (!Rooster)
	{
		return;
	}

	const FVector RoosterLoc = Rooster->GetActorLocation();
	const FRotator RoosterRot = Rooster->GetActorRotation();
	const FVector Forward = RoosterRot.Vector();
	const FVector Right = FRotationMatrix(RoosterRot).GetScaledAxis(EAxis::Y);

	// 3. Spawn or place the circular ring canvas mat right at the rooster's feet
	FVector MatLoc = RoosterLoc;
	FHitResult GroundHit;
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(MatGroundTrace), false, Rooster);
	if (World->LineTraceSingleByChannel(GroundHit, RoosterLoc + FVector(0.0f, 0.0f, 50.0f), RoosterLoc - FVector(0.0f, 0.0f, 300.0f), ECC_Visibility, TraceParams))
	{
		MatLoc.Z = GroundHit.Location.Z + 1.2f;
	}
	else
	{
		MatLoc.Z -= 88.0f;
	}

	if (!RingCanvasPedestalActor || !IsValid(RingCanvasPedestalActor))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* MeshActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), MatLoc, FRotator::ZeroRotator, SpawnParams);
		if (MeshActor)
		{
			RingCanvasPedestalActor = MeshActor;
			UStaticMeshComponent* SMC = MeshActor->GetStaticMeshComponent();
			if (SMC)
			{
				UStaticMesh* CylinderMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")));
				if (CylinderMesh)
				{
					SMC->SetStaticMesh(CylinderMesh);
					SMC->SetRelativeScale3D(FVector(2.7f, 2.7f, 0.004f));
				}

				UTexture2D* RingTex = FImageUtils::ImportFileAsTexture2D(FPaths::ProjectContentDir() / TEXT("UI/T_LonaRingBoxeo.png"));
				if (RingTex)
				{
					RingTex->AddToRoot();
					UMaterialInterface* BaseMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Characters/GalloBlanco/Textures/Material_001.Material_001")));
					if (!BaseMat)
					{
						BaseMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")));
					}
					if (BaseMat)
					{
						UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, MeshActor);
						if (DynMat)
						{
							DynMat->SetTextureParameterValue(FName(TEXT("DiffuseColorMap")), RingTex);
							DynMat->SetTextureParameterValue(FName(TEXT("Texture")), RingTex);
							DynMat->SetTextureParameterValue(FName(TEXT("BaseColor")), RingTex);
							DynMat->SetTextureParameterValue(FName(TEXT("Base Color")), RingTex);
							DynMat->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor::White);
							SMC->SetMaterial(0, DynMat);
						}
					}
				}
				SMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}
	}
	else
	{
		RingCanvasPedestalActor->SetActorLocation(MatLoc);
		RingCanvasPedestalActor->SetActorHiddenInGame(false);
	}

	// 4. Setup close-up Hero Camera with Depth-of-Field framing the rooster on the left-center
	// Positioned in front of the rooster, offset slightly to the right so rooster appears on left-center
	const FVector CamLoc = RoosterLoc + Forward * 230.0f + Right * 42.0f + FVector(0.0f, 0.0f, 18.0f);
	const FRotator CamRot = (RoosterLoc + FVector(0.0f, 0.0f, 14.0f) - CamLoc).Rotation();

	if (!CharacterSelectCameraActor || !IsValid(CharacterSelectCameraActor))
	{
		FActorSpawnParameters CamSpawnParams;
		CamSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		CharacterSelectCameraActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CamLoc, CamRot, CamSpawnParams);
	}
	else
	{
		CharacterSelectCameraActor->SetActorLocationAndRotation(CamLoc, CamRot);
	}

	if (CharacterSelectCameraActor)
	{
		UCameraComponent* CamComp = CharacterSelectCameraActor->GetCameraComponent();
		if (CamComp)
		{
			CamComp->FieldOfView = 62.0f;

			// Cinematic Depth-of-field: blur farm background, rooster in focus
			const float FocalDist = (CamLoc - RoosterLoc).Size();
			CamComp->PostProcessSettings.bOverride_DepthOfFieldFstop = true;
			CamComp->PostProcessSettings.DepthOfFieldFstop = 1.4f;
			CamComp->PostProcessSettings.bOverride_DepthOfFieldFocalDistance = true;
			CamComp->PostProcessSettings.DepthOfFieldFocalDistance = FocalDist;
			CamComp->PostProcessSettings.bOverride_DepthOfFieldSensorWidth = true;
			CamComp->PostProcessSettings.DepthOfFieldSensorWidth = 56.0f;
		}

		SetViewTarget(CharacterSelectCameraActor);
	}
}

void APanFightersPlayerController::CleanupCharacterSelectScene()
{
	// Restore hidden powerup actors
	for (TWeakObjectPtr<AActor>& WeakA : HiddenPowerupActors)
	{
		if (WeakA.IsValid())
		{
			WeakA->SetActorHiddenInGame(false);
			WeakA->SetActorEnableCollision(true);
		}
	}
	HiddenPowerupActors.Empty();

	// Destroy ring canvas pedestal actor
	if (RingCanvasPedestalActor && IsValid(RingCanvasPedestalActor))
	{
		RingCanvasPedestalActor->Destroy();
		RingCanvasPedestalActor = nullptr;
	}

	// Destroy character select camera actor if created
	if (CharacterSelectCameraActor && IsValid(CharacterSelectCameraActor))
	{
		CharacterSelectCameraActor->Destroy();
		CharacterSelectCameraActor = nullptr;
	}
}

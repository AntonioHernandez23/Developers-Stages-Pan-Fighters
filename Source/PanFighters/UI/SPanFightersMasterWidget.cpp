// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SPanFightersMasterWidget.h"
#include "PanFightersPlayerController.h"
#include "PanFighters.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBackgroundBlur.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "GameFramework/GameUserSettings.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

namespace PanFightersAudio
{
	static void LoadVolumes(float& OutMaster, float& OutMusic, float& OutSFX)
	{
		if (GConfig)
		{
			GConfig->GetFloat(TEXT("PanFighters.Audio"), TEXT("MasterVolume"), OutMaster, GGameUserSettingsIni);
			GConfig->GetFloat(TEXT("PanFighters.Audio"), TEXT("MusicVolume"), OutMusic, GGameUserSettingsIni);
			GConfig->GetFloat(TEXT("PanFighters.Audio"), TEXT("SFXVolume"), OutSFX, GGameUserSettingsIni);
			OutMaster = FMath::Clamp(OutMaster, 0.0f, 1.0f);
			OutMusic = FMath::Clamp(OutMusic, 0.0f, 1.0f);
			OutSFX = FMath::Clamp(OutSFX, 0.0f, 1.0f);
		}
	}

	static void SaveVolumes(float MasterVol, float MusicVol, float SFXVol)
	{
		if (GConfig)
		{
			GConfig->SetFloat(TEXT("PanFighters.Audio"), TEXT("MasterVolume"), FMath::Clamp(MasterVol, 0.0f, 1.0f), GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("PanFighters.Audio"), TEXT("MusicVolume"), FMath::Clamp(MusicVol, 0.0f, 1.0f), GGameUserSettingsIni);
			GConfig->SetFloat(TEXT("PanFighters.Audio"), TEXT("SFXVolume"), FMath::Clamp(SFXVol, 0.0f, 1.0f), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	static void LoadLanguage(bool& OutIsEnglish)
	{
		if (GConfig)
		{
			FString LangCode;
			if (GConfig->GetString(TEXT("PanFighters.Localization"), TEXT("Language"), LangCode, GGameUserSettingsIni))
			{
				OutIsEnglish = LangCode.Equals(TEXT("en"), ESearchCase::IgnoreCase);
			}
		}
	}

	static void SaveLanguage(bool bInIsEnglish)
	{
		if (GConfig)
		{
			GConfig->SetString(TEXT("PanFighters.Localization"), TEXT("Language"), bInIsEnglish ? TEXT("en") : TEXT("es"), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}

	static USoundMix* GetOrCreateTransientMix(TWeakObjectPtr<USoundMix>& CacheSlot)
	{
		if (!CacheSlot.IsValid())
		{
			USoundMix* NewMix = NewObject<USoundMix>(GetTransientPackage(), NAME_None, RF_Transient);
			NewMix->AddToRoot();
			CacheSlot = NewMix;
		}
		return CacheSlot.Get();
	}

	static void ApplyVolumes(UWorld* World, float MasterVol, float MusicVol, float SFXVol)
	{
		MasterVol = FMath::Clamp(MasterVol, 0.0f, 1.0f);
		MusicVol = FMath::Clamp(MusicVol, 0.0f, 1.0f);
		SFXVol = FMath::Clamp(SFXVol, 0.0f, 1.0f);

		if (World)
		{
			static TWeakObjectPtr<USoundMix> MasterMixCache;
			static TWeakObjectPtr<USoundMix> SFXMixCache;
			static TWeakObjectPtr<UWorld> LastPushedWorld;

			USoundMix* MasterMix = GetOrCreateTransientMix(MasterMixCache);
			USoundMix* SFXMix = GetOrCreateTransientMix(SFXMixCache);

			if (LastPushedWorld.Get() != World)
			{
				UGameplayStatics::PushSoundMixModifier(World, MasterMix);
				UGameplayStatics::PushSoundMixModifier(World, SFXMix);
				LastPushedWorld = World;
			}

			if (USoundClass* MasterClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/Master.Master")))
			{
				UGameplayStatics::SetSoundMixClassOverride(World, MasterMix, MasterClass, MasterVol, 1.0f, 0.01f, true);
			}
			if (USoundClass* SFXClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/SFX.SFX")))
			{
				UGameplayStatics::SetSoundMixClassOverride(World, SFXMix, SFXClass, SFXVol, 1.0f, 0.01f, true);
			}
			if (USoundClass* VoiceClass = LoadObject<USoundClass>(nullptr, TEXT("/Engine/EngineSounds/Voice.Voice")))
			{
				UGameplayStatics::SetSoundMixClassOverride(World, SFXMix, VoiceClass, SFXVol, 1.0f, 0.01f, true);
			}
		}

		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			UAudioComponent* AudioComp = *It;
			if (!AudioComp || !IsValid(AudioComp))
			{
				continue;
			}
			if (World && AudioComp->GetWorld() != World)
			{
				continue;
			}

			const USoundBase* SoundAsset = AudioComp->Sound.Get();
			const FString SoundName = SoundAsset ? SoundAsset->GetName() : FString();
			const bool bIsMusic = AudioComp->bIsUISound && (SoundName.StartsWith(TEXT("Mus_")) || SoundName.Contains(TEXT("at-war")));
			if (bIsMusic)
			{
				AudioComp->SetVolumeMultiplier(MusicVol);
				if (MusicVol > KINDA_SMALL_NUMBER && !AudioComp->IsPlaying())
				{
					AudioComp->Play();
				}
			}
			else if (SoundAsset)
			{
				const USoundClass* SC = SoundAsset->GetSoundClass();
				if (!SC || SC->GetName() == TEXT("Master"))
				{
					AudioComp->SetVolumeMultiplier(SFXVol);
				}
			}
		}
	}
}

void SPanFightersMasterWidget::Construct(const FArguments& InArgs)
{
	OwnerPC = InArgs._OwnerPlayerController;
	SplashTotalDuration = FMath::Max(1.0f, InArgs._SplashDuration);
	SplashElapsedTime = 0.0f;
	SplashProgress = 0.0f;
	bSplashFinished = false;

	// Restore and apply persisted audio, display, quality, and language settings at startup
	PanFightersAudio::LoadVolumes(MasterVolume, MusicVolume, SFXVolume);
	bAudioMuted = (MasterVolume <= KINDA_SMALL_NUMBER);
	SavedMasterBeforeMute = (MasterVolume > KINDA_SMALL_NUMBER) ? MasterVolume : 1.0f;
	PanFightersAudio::LoadLanguage(bIsEnglish);
	PanFightersAudio::ApplyVolumes(OwnerPC.IsValid() ? OwnerPC->GetWorld() : nullptr, MasterVolume, MusicVolume, SFXVolume);
	LoadDisplayAndQualitySettings();

	// Load preview images for each stage
	LoadStageImages();

	ChildSlot
	[
		SAssignNew(RootOverlay, SOverlay)

		// Main dynamic screen container (Splash, MainMenu, Options, StageSelect, InGameMenu)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(ScreenContentContainer, SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.04f, 0.035f, 0.03f, 1.0f))
			.Padding(0.0f)
		]

		// In-Game Top Cinematic Banner (fades out after entering stage)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Top)
		.Padding(0.0f, 40.0f, 0.0f, 0.0f)
		[
			SAssignNew(InGameBannerContainer, SBorder)
			.Visibility(EVisibility::Collapsed)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.05f, 0.04f, 0.03f, 0.92f))
			.Padding(FMargin(32.0f, 14.0f))
		]

		// In-Game Bottom-Right Helper Badge ([ESC] / [P] Menu)
		+ SOverlay::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 30.0f, 25.0f)
		[
			SAssignNew(InGameHUDHelperContainer, SBorder)
			.Visibility(EVisibility::Collapsed)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.08f, 0.06f, 0.04f, 0.85f))
			.Padding(FMargin(18.0f, 8.0f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return Tr(TEXT("⚙ [ESC] o [P]  Pausar Juego"), TEXT("⚙ [ESC] or [P]  Pause Game")); })
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.80f, 0.40f, 0.95f)))
			]
		]

		// In-Game Fighting Combat HUD (Health bars, Avatars, VS emblem, Controls)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(InGameFightHUDWidget, SOverlay)
			.Visibility(this, &SPanFightersMasterWidget::GetInGameFightHUDVisibility)

			// Top Health Bars & VS Emblem
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 20.0f, 0.0f, 0.0f)
			[
				BuildInGameFightHUD()
			]

			// Bottom Combat Controls Helper Pill
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f, 0.0f, 0.0f, 18.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.06f, 0.05f, 0.04f, 0.85f))
				.Padding(FMargin(20.0f, 7.0f))
				[
					SNew(STextBlock)
					.Text_Lambda([this]() { return Tr(TEXT("🥊 [CLIC IZQ] Golpear / Jab   •   [CLIC DER] Gancho   •   [ESPACIO] Saltar   •   [ESC / P] Pausa"), TEXT("🥊 [LEFT CLICK] Jab / Strike   •   [RIGHT CLICK] Hook   •   [SPACE] Jump   •   [ESC / P] Pause")); })
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.76f, 0.95f)))
				]
			]
		]

		// Red Damage / Hit Reaction Flash Overlay (Topmost visual layer)
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(HitReactionContainer, SBorder)
			.Visibility(EVisibility::HitTestInvisible)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(this, &SPanFightersMasterWidget::GetHitReactionColor)
			.Padding(0.0f)
		]
	];

	// Start at Splash screen
	SetScreenState(EPanFightersScreenState::Splash);
}

void SPanFightersMasterWidget::LoadStageImages()
{
	const FString ContentDir = FPaths::ProjectContentDir() / TEXT("UI/Stages/");

	auto LoadImageBrush = [](const FString& Path, const FVector2D& Size) -> TSharedPtr<FSlateBrush>
	{
		FString ResolvedPath = Path;
		if (!FPaths::FileExists(ResolvedPath))
		{
			ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/UI/Stages/"), FPaths::GetCleanFilename(Path));
		}
		if (!FPaths::FileExists(ResolvedPath))
		{
			ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/UI/Splash/"), FPaths::GetCleanFilename(Path));
		}
		if (!FPaths::FileExists(ResolvedPath))
		{
			ResolvedPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Content/UI/"), FPaths::GetCleanFilename(Path));
		}

		if (FPaths::FileExists(ResolvedPath))
		{
			if (UTexture2D* LoadedTex = FImageUtils::ImportFileAsTexture2D(ResolvedPath))
			{
				LoadedTex->AddToRoot(); // Prevent garbage collection
				return MakeShareable(new FSlateImageBrush(LoadedTex, Size));
			}
		}
		return nullptr;
	};

	BrushRanchito = LoadImageBrush(ContentDir / TEXT("Img_ElRanchito.jpg"), FVector2D(280.0f, 135.0f));
	BrushCampoLlano = LoadImageBrush(ContentDir / TEXT("Img_ElCampoLlano.jpg"), FVector2D(280.0f, 135.0f));
	BrushSarten = LoadImageBrush(ContentDir / TEXT("Img_ElSarten.jpg"), FVector2D(280.0f, 135.0f));
	BrushSplash = LoadImageBrush(FPaths::ProjectContentDir() / TEXT("UI/Splash/Img_Splash.jpg"), FVector2D(1024.0f, 572.0f));

	const FString UIContentDir = FPaths::ProjectContentDir() / TEXT("UI/");
	BrushPolloBlanco = LoadImageBrush(UIContentDir / TEXT("Img_PolloBlanco.jpg"), FVector2D(256.0f, 256.0f));
	BrushPolloChocolate = LoadImageBrush(UIContentDir / TEXT("Img_PolloChocolate.jpg"), FVector2D(256.0f, 256.0f));
	BrushPolloAmarillo = LoadImageBrush(UIContentDir / TEXT("Img_PolloAmarillo.jpg"), FVector2D(256.0f, 256.0f));
	BrushGalloGato = LoadImageBrush(UIContentDir / TEXT("Img_GalloGato.jpg"), FVector2D(256.0f, 256.0f));
	BrushTitleBanner = LoadImageBrush(UIContentDir / TEXT("T_BannerTituloMadera.png"), FVector2D(495.0f, 88.0f));
	BrushCardBg = LoadImageBrush(UIContentDir / TEXT("T_CardBackground.png"), FVector2D(280.0f, 110.0f));
	BrushGridBg = LoadImageBrush(UIContentDir / TEXT("T_GridBackground.png"), FVector2D(450.0f, 430.0f));
	BrushSlotLocked = LoadImageBrush(UIContentDir / TEXT("T_SlotLocked.png"), FVector2D(120.0f, 120.0f));
	BrushNavButton = LoadImageBrush(UIContentDir / TEXT("T_BotonMaderaNav.png"), FVector2D(240.0f, 52.0f));
	BrushMainMenuBg = LoadImageBrush(UIContentDir / TEXT("T_MainMenuBg.jpg"), FVector2D(1024.0f, 572.0f));
}

void SPanFightersMasterWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Splash 15-second loading bar progression
	if (CurrentState == EPanFightersScreenState::Splash && !bSplashFinished)
	{
		SplashElapsedTime += InDeltaTime;
		SplashProgress = FMath::Clamp(SplashElapsedTime / SplashTotalDuration, 0.0f, 1.0f);

		if (SplashProgress >= 1.0f)
		{
			bSplashFinished = true;
			// Transition to Main Menu when 15 seconds finish
			SetScreenState(EPanFightersScreenState::MainMenu);
		}
	}

	// Automatic language switch after brief delay ("a los segundos")
	if (bPendingLanguageSwitch)
	{
		LanguageSwitchTimer += InDeltaTime;
		if (LanguageSwitchTimer >= LanguageSwitchDelay)
		{
			bPendingLanguageSwitch = false;
			LanguageSwitchTimer = 0.0f;
			bIsEnglish = bTargetLanguageIsEnglish;
			PanFightersAudio::SaveLanguage(bIsEnglish);
			if (ScreenContentContainer.IsValid())
			{
				SetScreenState(CurrentState);
			}
		}
	}

	// InGame cinematic banner fade timer
	if (bShowInGameBanner)
	{
		BannerElapsedTime += InDeltaTime;
		if (BannerElapsedTime >= BannerDuration)
		{
			bShowInGameBanner = false;
			if (InGameBannerContainer.IsValid())
			{
				InGameBannerContainer->SetVisibility(EVisibility::Collapsed);
			}
		}
	}

	// Decay hit reaction screen flash
	if (HitReactionAlpha > 0.0f)
	{
		HitReactionAlpha = FMath::Max(0.0f, HitReactionAlpha - InDeltaTime * 2.4f);
	}

	if (PlayerHealthBarShakeTime > 0.0f)
	{
		PlayerHealthBarShakeTime = FMath::Max(0.0f, PlayerHealthBarShakeTime - InDeltaTime);
	}

	// Smoothly interpolate ghost damage bars towards target health
	const float TargetPlayerHP = OwnerPC.IsValid() ? OwnerPC->GetPlayerCurrentHealth() : 100.0f;
	const float TargetRivalHP = OwnerPC.IsValid() ? OwnerPC->GetRivalCurrentHealth() : 100.0f;

	if (PlayerGhostHealth > TargetPlayerHP)
	{
		PlayerGhostHealth = FMath::FInterpTo(PlayerGhostHealth, TargetPlayerHP, InDeltaTime, 3.5f);
	}
	else
	{
		PlayerGhostHealth = TargetPlayerHP;
	}

	if (RivalGhostHealth > TargetRivalHP)
	{
		RivalGhostHealth = FMath::FInterpTo(RivalGhostHealth, TargetRivalHP, InDeltaTime, 3.5f);
	}
	else
	{
		RivalGhostHealth = TargetRivalHP;
	}
}

FReply SPanFightersMasterWidget::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (CurrentState == EPanFightersScreenState::CharacterSelect)
	{
		if (Key == EKeys::Escape)
		{
			if (OwnerPC.IsValid())
			{
				OwnerPC->ReturnToMainMenu();
				return FReply::Handled();
			}
		}
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar)
		{
			if (OwnerPC.IsValid())
			{
				OwnerPC->ConfirmCharacterAndProceed();
				return FReply::Handled();
			}
		}
		else if (Key == EKeys::Left || Key == EKeys::A)
		{
			if (OwnerPC.IsValid())
			{
				OwnerPC->SetPreviewCharacter(EPanFightersCharacter::PolloBlanco);
				return FReply::Handled();
			}
		}
		else if (Key == EKeys::Right || Key == EKeys::D)
		{
			if (OwnerPC.IsValid())
			{
				OwnerPC->SetPreviewCharacter(EPanFightersCharacter::PolloChocolate);
				return FReply::Handled();
			}
		}
	}

	if (CurrentState == EPanFightersScreenState::OptionsMenu && Key == EKeys::Escape)
	{
		if (OwnerPC.IsValid())
		{
			OwnerPC->ReturnToMainMenu();
			return FReply::Handled();
		}
	}

	if (Key == EKeys::Escape || Key == EKeys::P || Key == EKeys::M)
	{
		if (CurrentState == EPanFightersScreenState::InGame || CurrentState == EPanFightersScreenState::InGameMenu)
		{
			if (OwnerPC.IsValid())
			{
				OwnerPC->ToggleInGameMenu();
				return FReply::Handled();
			}
		}
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SPanFightersMasterWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ((CurrentState == EPanFightersScreenState::CharacterSelect || CurrentState == EPanFightersScreenState::MatchResult) &&
		MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDraggingRooster = true;
		LastMouseDragPos = MouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}
	return SCompoundWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply SPanFightersMasterWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsDraggingRooster && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDraggingRooster = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return SCompoundWidget::OnMouseButtonUp(MyGeometry, MouseEvent);
}

FReply SPanFightersMasterWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (CurrentState == EPanFightersScreenState::CharacterSelect || CurrentState == EPanFightersScreenState::MatchResult)
	{
		if (bIsDraggingRooster)
		{
			const FVector2D CurrentPos = MouseEvent.GetScreenSpacePosition();
			const float DeltaX = CurrentPos.X - LastMouseDragPos.X;
			LastMouseDragPos = CurrentPos;

			if (OwnerPC.IsValid())
			{
				OwnerPC->RotatePreviewRooster(-DeltaX * 0.75f);
			}
			return FReply::Handled();
		}
		else if (CurrentState == EPanFightersScreenState::CharacterSelect && OwnerPC.IsValid())
		{
			OwnerPC->UpdatePreviewRoosterHover(MouseEvent.GetScreenSpacePosition());
		}
	}
	return SCompoundWidget::OnMouseMove(MyGeometry, MouseEvent);
}

void SPanFightersMasterWidget::SetScreenState(EPanFightersScreenState NewState)
{
	CurrentState = NewState;

	if (OwnerPC.IsValid())
	{
		OwnerPC->OnScreenStateChanged(NewState);
	}

	if (!ScreenContentContainer.IsValid())
	{
		return;
	}

	// In-game helper badge visibility
	if (InGameHUDHelperContainer.IsValid())
	{
		InGameHUDHelperContainer->SetVisibility(NewState == EPanFightersScreenState::InGame ? EVisibility::HitTestInvisible : EVisibility::Collapsed);
	}

	switch (NewState)
	{
	case EPanFightersScreenState::Splash:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor(0.04f, 0.035f, 0.03f, 1.0f));
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildSplashScreen());
		break;

	case EPanFightersScreenState::MainMenu:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor::Transparent);
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildMainMenu());
		break;

	case EPanFightersScreenState::OptionsMenu:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor::Transparent);
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildOptionsMenu());
		break;

	case EPanFightersScreenState::CharacterSelect:
		// Translucent/clean container so the 3D rooster in El Ranchito is fully visible and interactable!
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor::Transparent);
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildCharacterSelectMenu());
		break;

	case EPanFightersScreenState::StageSelect:
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor(0.05f, 0.04f, 0.035f, 0.96f));
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildStageSelectMenu());
		break;

	case EPanFightersScreenState::InGame:
		// When viewing the stage in-game, collapse the main menu overlay so the 3D first-person scene is fully visible!
		ScreenContentContainer->SetVisibility(EVisibility::Collapsed);
		PlayerGhostHealth = 100.0f;
		RivalGhostHealth = 100.0f;
		HitReactionAlpha = 0.0f;
		PlayerHealthBarShakeTime = 0.0f;
		break;

	case EPanFightersScreenState::InGameMenu:
		// Translucent container for the 40% blur/dimmed pause menu
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor::Transparent);
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		ScreenContentContainer->SetContent(BuildInGameMenu());
		break;

	case EPanFightersScreenState::MatchResult:
		// Translucent container for the 40% blur/dimmed victory/defeat result menu
		ScreenContentContainer->SetBorderBackgroundColor(FLinearColor::Transparent);
		ScreenContentContainer->SetVisibility(EVisibility::Visible);
		{
			const bool bVictory = OwnerPC.IsValid() ? OwnerPC->IsLastMatchVictory() : true;
			ScreenContentContainer->SetContent(BuildMatchResultMenu(bVictory));
		}
		break;

	default:
		break;
	}
}

void SPanFightersMasterWidget::NotifyStageEntered(EPanFightersStage Stage, const FText& InStageName, const FText& InStageSubtitle)
{
	switch (Stage)
	{
	case EPanFightersStage::Stage1_ElRanchito:
		ActiveStageName = Tr(TEXT("El Ranchito"), TEXT("El Ranchito"));
		ActiveStageSubtitle = Tr(TEXT("Bohío Campesino Tradicional y Ruedo de las Aves"), TEXT("Traditional Countryside Hut & Rooster Ring"));
		break;
	case EPanFightersStage::Stage2_ElCampoLlano:
		ActiveStageName = Tr(TEXT("El Campo Llano"), TEXT("El Campo Llano"));
		ActiveStageSubtitle = Tr(TEXT("La Sabana y Potrero a Campo Abierto"), TEXT("Open-Field Savanna & Pasture"));
		break;
	case EPanFightersStage::Stage3_ElSarten:
		ActiveStageName = Tr(TEXT("El Sartén"), TEXT("El Sartén (The Frying Pan)"));
		ActiveStageSubtitle = Tr(TEXT("La Cocina Campesina al Fuego Vivo"), TEXT("Countryside Kitchen Over Open Fire"));
		break;
	default:
		ActiveStageName = InStageName;
		ActiveStageSubtitle = InStageSubtitle;
		break;
	}
	NotifyRoundBanner(
		FText::Format(Tr(TEXT("✦  ESCENARIO: {0}  ✦"), TEXT("✦  STAGE: {0}  ✦")), ActiveStageName),
		ActiveStageSubtitle,
		Tr(TEXT("Asalto 1 de 3 • El primero en ganar 2 asaltos triunfa"), TEXT("Round 1 of 3 • First to win 2 rounds triumphs")),
		4.5f
	);
}

void SPanFightersMasterWidget::NotifyRoundBanner(const FText& Headline, const FText& Subline, const FText& FooterNote, float DisplayDuration)
{
	BannerElapsedTime = 0.0f;
	BannerDuration = FMath::Max(0.5f, DisplayDuration);
	bShowInGameBanner = true;

	if (InGameBannerContainer.IsValid())
	{
		InGameBannerContainer->SetVisibility(EVisibility::HitTestInvisible);
		TSharedRef<SVerticalBox> BannerBox = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(Headline)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 1.0f)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(Subline)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.85f, 0.80f, 0.90f)))
			];

		if (!FooterNote.IsEmpty())
		{
			BannerBox->AddSlot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FooterNote)
				.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.78f, 0.62f, 0.90f)))
			];
		}

		InGameBannerContainer->SetContent(BannerBox);
	}
}

void SPanFightersMasterWidget::HideInGameBanner()
{
	bShowInGameBanner = false;
	BannerElapsedTime = 0.0f;
	if (InGameBannerContainer.IsValid())
	{
		InGameBannerContainer->SetVisibility(EVisibility::Collapsed);
	}
}

// --------------------------------------------------------------------------
// SPLASH SCREEN BUILDER (5 Seconds loading bar con Artwork de Pan Fighters)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildSplashScreen()
{
	return SNew(SOverlay)

	// Full Cover Background Splash Image
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		BrushSplash.IsValid()
		? SNew(SImage)
			.Image(BrushSplash.Get())
		: SNew(SImage)
			.Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.04f, 0.035f, 0.03f, 1.0f)))
	]

	// Dark gradient overlay at the bottom for crystal-clear loading bar legibility
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Bottom)
	[
		SNew(SBox)
		.HeightOverride(160.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f))
			.Padding(0.0f)
		]
	]

	// Bottom Loading Section (Barra de carga de 5 segundos)
	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Bottom)
	.Padding(0.0f, 0.0f, 0.0f, 40.0f)
	[
		SNew(SVerticalBox)

		// Dynamic Loading Message
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(STextBlock)
			.Text(this, &SPanFightersMasterWidget::GetLoadingStatusText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.88f, 1.0f)))
		]

		// Progress Bar Frame Container (760px wide, 24px high)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(760.0f)
			.HeightOverride(24.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.10f, 0.08f, 0.06f, 1.0f))
				.Padding(FMargin(2.5f))
				[
					SNew(SProgressBar)
					.Percent(this, &SPanFightersMasterWidget::GetProgressBarPercent)
					.FillColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.65f, 0.15f, 1.0f)))
					.BorderPadding(FVector2D::ZeroVector)
				]
			]
		]

		// Percentage Readout
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SPanFightersMasterWidget::GetProgressPercentText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.40f, 1.0f)))
		]
	];
}

// --------------------------------------------------------------------------
// MAIN MENU BUILDER (Jugar, Opciones, Salir)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildMainMenu()
{
	return SNew(SScaleBox)
	.Stretch(EStretch::ScaleToFit)
	[
		SNew(SBox)
		.WidthOverride(1920.0f)
		.HeightOverride(1080.0f)
		[
			SNew(SOverlay)

			// 1. FULLSCREEN KEY ART (contains 3D title logo embedded in top center)
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Image(BrushMainMenuBg.IsValid() ? BrushMainMenuBg.Get() : (BrushSplash.IsValid() ? BrushSplash.Get() : FCoreStyle::Get().GetBrush("WhiteBrush")))
			]

			// 2. REORGANIZED MENU BUTTONS (Lower-left stack, leaving art, roosters, and logo completely clear!)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(110.0f, 0.0f, 0.0f, 95.0f)
			[
				SNew(SVerticalBox)

				// OPTION 1: JUGAR
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[
					SNew(SBox)
					.WidthOverride(290.0f)
					.HeightOverride(64.0f)
					[
						SNew(SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.ContentPadding(FMargin(0.0f))
						.OnClicked(FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnPlayClicked))
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SImage)
								.Image(BrushNavButton.IsValid() ? BrushNavButton.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
								.ColorAndOpacity(BrushNavButton.IsValid() ? FLinearColor::White : FLinearColor(0.95f, 0.65f, 0.15f, 1.0f))
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(Tr(TEXT("▶  JUGAR"), TEXT("▶  PLAY")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.94f, 0.72f, 1.0f)))
								.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
								.ShadowOffset(FVector2D(1.5f, 1.5f))
							]
						]
					]
				]

				// OPTION 2: OPCIONES
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[
					SNew(SBox)
					.WidthOverride(290.0f)
					.HeightOverride(64.0f)
					[
						SNew(SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.ContentPadding(FMargin(0.0f))
						.OnClicked(FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnOptionsClicked))
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SImage)
								.Image(BrushNavButton.IsValid() ? BrushNavButton.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
								.ColorAndOpacity(BrushNavButton.IsValid() ? FLinearColor::White : FLinearColor(0.75f, 0.50f, 0.20f, 1.0f))
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(Tr(TEXT("⚙  OPCIONES"), TEXT("⚙  OPTIONS")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.90f, 0.68f, 1.0f)))
								.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
								.ShadowOffset(FVector2D(1.5f, 1.5f))
							]
						]
					]
				]

				// OPTION 3: SALIR
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 20.0f)
				[
					SNew(SBox)
					.WidthOverride(290.0f)
					.HeightOverride(64.0f)
					[
						SNew(SButton)
						.ButtonStyle(FCoreStyle::Get(), "NoBorder")
						.ContentPadding(FMargin(0.0f))
						.OnClicked(FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnQuitClicked))
						[
							SNew(SOverlay)

							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SImage)
								.Image(BrushNavButton.IsValid() ? BrushNavButton.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
								.ColorAndOpacity(BrushNavButton.IsValid() ? FLinearColor::White : FLinearColor(0.65f, 0.22f, 0.18f, 1.0f))
							]

							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(Tr(TEXT("✕  SALIR"), TEXT("✕  QUIT")))
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.80f, 1.0f)))
								.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
								.ShadowOffset(FVector2D(1.5f, 1.5f))
							]
						]
					]
				]

				// Footer
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("Pan Fighters © 2026 • Unreal Engine 5")))
					.Font(FCoreStyle::GetDefaultFontStyle("Italic", 12))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.85f, 0.75f)))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// OPTIONS MENU BUILDER & SETTINGS HELPERS
// --------------------------------------------------------------------------
void SPanFightersMasterWidget::LoadDisplayAndQualitySettings()
{
	if (GEngine)
	{
		if (UGameUserSettings* GUS = GEngine->GetGameUserSettings())
		{
			const int32 Scalability = GUS->GetOverallScalabilityLevel();
			GraphicsQualityLevel = (Scalability >= 0) ? FMath::Clamp(Scalability, 0, 3) : 3;

			switch (GUS->GetFullscreenMode())
			{
			case EWindowMode::Fullscreen:
				DisplayWindowModeIndex = 0;
				break;
			case EWindowMode::WindowedFullscreen:
				DisplayWindowModeIndex = 1;
				break;
			default:
				DisplayWindowModeIndex = 2;
				break;
			}

			const float RawCap = GUS->GetFrameRateLimit();
			if (RawCap > 0.0f && RawCap <= 75.0f)
			{
				FrameRateCapValue = 60;
			}
			else if (RawCap > 75.0f && RawCap <= 140.0f)
			{
				FrameRateCapValue = 120;
			}
			else
			{
				FrameRateCapValue = 0;
			}
		}
	}

	if (GConfig)
	{
		GConfig->GetInt(TEXT("PanFighters.Video"), TEXT("QualityLevel"), GraphicsQualityLevel, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("PanFighters.Video"), TEXT("WindowMode"), DisplayWindowModeIndex, GGameUserSettingsIni);
		GConfig->GetInt(TEXT("PanFighters.Video"), TEXT("FrameRateCap"), FrameRateCapValue, GGameUserSettingsIni);
		GraphicsQualityLevel = FMath::Clamp(GraphicsQualityLevel, 0, 3);
		DisplayWindowModeIndex = FMath::Clamp(DisplayWindowModeIndex, 0, 2);
		if (FrameRateCapValue != 0 && FrameRateCapValue != 60 && FrameRateCapValue != 120)
		{
			FrameRateCapValue = 0;
		}
	}
}

void SPanFightersMasterWidget::ApplyGraphicsQuality(int32 NewLevel)
{
	GraphicsQualityLevel = FMath::Clamp(NewLevel, 0, 3);

	if (GEngine)
	{
		if (UGameUserSettings* GUS = GEngine->GetGameUserSettings())
		{
			GUS->SetOverallScalabilityLevel(GraphicsQualityLevel);
			GUS->ApplyNonResolutionSettings();
			GUS->SaveSettings();
		}
	}

	if (GConfig)
	{
		GConfig->SetInt(TEXT("PanFighters.Video"), TEXT("QualityLevel"), GraphicsQualityLevel, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void SPanFightersMasterWidget::ApplyWindowMode(int32 NewModeIndex)
{
	DisplayWindowModeIndex = FMath::Clamp(NewModeIndex, 0, 2);
	const EWindowMode::Type TargetMode =
		(DisplayWindowModeIndex == 0) ? EWindowMode::Fullscreen :
		(DisplayWindowModeIndex == 1) ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;

	if (GEngine)
	{
		if (UGameUserSettings* GUS = GEngine->GetGameUserSettings())
		{
			GUS->SetFullscreenMode(TargetMode);
			const UWorld* World = OwnerPC.IsValid() ? OwnerPC->GetWorld() : nullptr;
			const bool bIsPIE = (World && World->IsPlayInEditor());
			if (!bIsPIE)
			{
				GUS->ApplyResolutionSettings(false);
			}
			GUS->SaveSettings();
		}
	}

	if (GConfig)
	{
		GConfig->SetInt(TEXT("PanFighters.Video"), TEXT("WindowMode"), DisplayWindowModeIndex, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void SPanFightersMasterWidget::ApplyFrameRateCap(int32 NewCapValue)
{
	FrameRateCapValue = (NewCapValue == 60 || NewCapValue == 120) ? NewCapValue : 0;

	if (GEngine)
	{
		if (UGameUserSettings* GUS = GEngine->GetGameUserSettings())
		{
			GUS->SetFrameRateLimit(static_cast<float>(FrameRateCapValue));
			GUS->ApplyNonResolutionSettings();
			GUS->SaveSettings();
		}
	}

	if (GConfig)
	{
		GConfig->SetInt(TEXT("PanFighters.Video"), TEXT("FrameRateCap"), FrameRateCapValue, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void SPanFightersMasterWidget::ResetOptionsToDefaults()
{
	MasterVolume = 1.0f;
	MusicVolume = 0.8f;
	SFXVolume = 0.9f;
	bAudioMuted = false;
	SavedMasterBeforeMute = 1.0f;

	UWorld* World = OwnerPC.IsValid() ? OwnerPC->GetWorld() : nullptr;
	PanFightersAudio::ApplyVolumes(World, MasterVolume, MusicVolume, SFXVolume);
	PanFightersAudio::SaveVolumes(MasterVolume, MusicVolume, SFXVolume);

	ApplyGraphicsQuality(3);
	ApplyWindowMode(1);
	ApplyFrameRateCap(0);
}

TSharedRef<SWidget> SPanFightersMasterWidget::BuildOptionsMenu()
{
	PanFightersAudio::LoadVolumes(MasterVolume, MusicVolume, SFXVolume);
	bAudioMuted = (MasterVolume <= KINDA_SMALL_NUMBER);
	if (MasterVolume > KINDA_SMALL_NUMBER)
	{
		SavedMasterBeforeMute = MasterVolume;
	}
	PanFightersAudio::LoadLanguage(bIsEnglish);
	LoadDisplayAndQualitySettings();

	TWeakPtr<SPanFightersMasterWidget> WeakSelf = StaticCastSharedRef<SPanFightersMasterWidget>(AsShared());

	auto ApplyAndSaveAudio = [WeakSelf]()
	{
		if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
		{
			Pinned->bAudioMuted = (Pinned->MasterVolume <= KINDA_SMALL_NUMBER);
			if (Pinned->MasterVolume > KINDA_SMALL_NUMBER)
			{
				Pinned->SavedMasterBeforeMute = Pinned->MasterVolume;
			}
			UWorld* World = Pinned->OwnerPC.IsValid() ? Pinned->OwnerPC->GetWorld() : nullptr;
			PanFightersAudio::ApplyVolumes(World, Pinned->MasterVolume, Pinned->MusicVolume, Pinned->SFXVolume);
			PanFightersAudio::SaveVolumes(Pinned->MasterVolume, Pinned->MusicVolume, Pinned->SFXVolume);
		}
	};

	auto MakeSectionHeader = [](const FText& HeaderTitle) -> TSharedRef<SWidget>
	{
		return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(HeaderTitle)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.30f, 1.0f)))
			.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
			.ShadowOffset(FVector2D(1.0f, 1.0f))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 16.0f)
		[
			SNew(SBox)
			.HeightOverride(2.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.86f, 0.62f, 0.16f, 0.65f))
			]
		];
	};

	auto MakeVolumeRow = [WeakSelf, ApplyAndSaveAudio](const FText& LabelText, float SPanFightersMasterWidget::* VolumeMember, bool bShowMuteButton) -> TSharedRef<SWidget>
	{
		TSharedRef<SHorizontalBox> ControlsRow = SNew(SHorizontalBox);

		if (bShowMuteButton)
		{
			ControlsRow->AddSlot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(58.0f)
				.HeightOverride(32.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity_Lambda([WeakSelf]()
					{
						if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
						{
							return FSlateColor(Pinned->bAudioMuted
								? FLinearColor(0.76f, 0.20f, 0.16f, 1.0f)
								: FLinearColor(0.24f, 0.18f, 0.13f, 1.0f));
						}
						return FSlateColor(FLinearColor(0.24f, 0.18f, 0.13f, 1.0f));
					})
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.OnClicked_Lambda([WeakSelf, ApplyAndSaveAudio]()
					{
						if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
						{
							if (Pinned->bAudioMuted)
							{
								Pinned->MasterVolume = (Pinned->SavedMasterBeforeMute > KINDA_SMALL_NUMBER) ? Pinned->SavedMasterBeforeMute : 1.0f;
							}
							else
							{
								Pinned->SavedMasterBeforeMute = (Pinned->MasterVolume > KINDA_SMALL_NUMBER) ? Pinned->MasterVolume : 1.0f;
								Pinned->MasterVolume = 0.0f;
							}
							ApplyAndSaveAudio();
						}
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text_Lambda([WeakSelf]()
						{
							if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
							{
								if (Pinned->bAudioMuted)
								{
									return Pinned->Tr(TEXT("MUDO"), TEXT("MUTED"));
								}
								return Pinned->Tr(TEXT("MUTE"), TEXT("MUTE"));
							}
							return FText::FromString(TEXT("MUTE"));
						})
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			];
		}

		ControlsRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(32.0f)
			.HeightOverride(32.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.26f, 0.19f, 0.13f, 1.0f)))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([WeakSelf, VolumeMember, ApplyAndSaveAudio]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						float& Vol = Pinned.Get()->*VolumeMember;
						Vol = FMath::Clamp(FMath::RoundToFloat((Vol - 0.10f) * 20.0f) / 20.0f, 0.0f, 1.0f);
						ApplyAndSaveAudio();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("-")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.92f, 0.72f, 1.0f)))
				]
			]
		];

		ControlsRow->AddSlot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f)
		[
			SNew(SSlider)
			.StepSize(0.05f)
			.SliderBarColor(FLinearColor(0.22f, 0.16f, 0.11f, 1.0f))
			.SliderHandleColor(FLinearColor(0.95f, 0.72f, 0.20f, 1.0f))
			.Value_Lambda([WeakSelf, VolumeMember]()
			{
				if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
				{
					return Pinned.Get()->*VolumeMember;
				}
				return 1.0f;
			})
			.OnValueChanged_Lambda([WeakSelf, VolumeMember, ApplyAndSaveAudio](float Val)
			{
				if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
				{
					Pinned.Get()->*VolumeMember = FMath::Clamp(Val, 0.0f, 1.0f);
					ApplyAndSaveAudio();
				}
			})
		];

		ControlsRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(32.0f)
			.HeightOverride(32.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.50f, 0.16f, 1.0f)))
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([WeakSelf, VolumeMember, ApplyAndSaveAudio]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						float& Vol = Pinned.Get()->*VolumeMember;
						Vol = FMath::Clamp(FMath::RoundToFloat((Vol + 0.10f) * 20.0f) / 20.0f, 0.0f, 1.0f);
						ApplyAndSaveAudio();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("+")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
				]
			]
		];

		ControlsRow->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(46.0f)
			.HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Text_Lambda([WeakSelf, VolumeMember]()
				{
					const float Vol = WeakSelf.IsValid() ? (WeakSelf.Pin().Get()->*VolumeMember) : 1.0f;
					return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(Vol * 100.0f)));
				})
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.32f, 1.0f)))
			]
		];

		return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.13f, 0.10f, 0.075f, 0.85f))
		.Padding(FMargin(14.0f, 10.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SNew(STextBlock)
				.Text(LabelText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.82f, 1.0f)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlsRow
			]
		];
	};

	auto MakePillButton = [](const FText& BtnText, TAttribute<FSlateColor> BgColor, FOnClicked OnClicked, float PillWidth = 0.0f) -> TSharedRef<SWidget>
	{
		TSharedRef<SBox> Box = SNew(SBox)
			.HeightOverride(34.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
				.ButtonColorAndOpacity(BgColor)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ContentPadding(FMargin(10.0f, 4.0f))
				.OnClicked(OnClicked)
				[
					SNew(STextBlock)
					.Text(BtnText)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
					.ColorAndOpacity(FSlateColor(FLinearColor::White))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
			];
		if (PillWidth > 0.0f)
		{
			Box->SetWidthOverride(PillWidth);
		}
		return Box;
	};

	auto MakeLabeledSettingRow = [](const FText& LabelText, TSharedRef<SWidget> ControlContent) -> TSharedRef<SWidget>
	{
		return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.13f, 0.10f, 0.075f, 0.85f))
		.Padding(FMargin(14.0f, 10.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(LabelText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.90f, 0.82f, 1.0f)))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ControlContent
			]
		];
	};

	// Quality selector pills (0=Low, 1=Medium, 2=High, 3=Epic)
	TSharedRef<SHorizontalBox> QualityPills = SNew(SHorizontalBox);
	const TCHAR* QualityEs[4] = { TEXT("BAJO"), TEXT("MEDIO"), TEXT("ALTO"), TEXT("ÉPICO") };
	const TCHAR* QualityEn[4] = { TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC") };
	for (int32 TierIdx = 0; TierIdx < 4; ++TierIdx)
	{
		QualityPills->AddSlot()
		.FillWidth(1.0f)
		.Padding(TierIdx > 0 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f)
		[
			MakePillButton(
				Tr(QualityEs[TierIdx], QualityEn[TierIdx]),
				TAttribute<FSlateColor>::CreateLambda([WeakSelf, TierIdx]()
				{
					const bool bActive = WeakSelf.IsValid() && WeakSelf.Pin()->GraphicsQualityLevel == TierIdx;
					return FSlateColor(bActive
						? FLinearColor(0.88f, 0.62f, 0.16f, 1.0f)
						: FLinearColor(0.22f, 0.18f, 0.14f, 0.95f));
				}),
				FOnClicked::CreateLambda([WeakSelf, TierIdx]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						Pinned->ApplyGraphicsQuality(TierIdx);
					}
					return FReply::Handled();
				})
			)
		];
	}

	// Display window mode pills (0=Fullscreen, 1=Borderless, 2=Windowed)
	TSharedRef<SHorizontalBox> WindowModePills = SNew(SHorizontalBox);
	const TCHAR* ModeEs[3] = { TEXT("COMPLETA"), TEXT("SIN BORDES"), TEXT("VENTANA") };
	const TCHAR* ModeEn[3] = { TEXT("FULLSCREEN"), TEXT("BORDERLESS"), TEXT("WINDOWED") };
	for (int32 ModeIdx = 0; ModeIdx < 3; ++ModeIdx)
	{
		WindowModePills->AddSlot()
		.FillWidth(1.0f)
		.Padding(ModeIdx > 0 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f)
		[
			MakePillButton(
				Tr(ModeEs[ModeIdx], ModeEn[ModeIdx]),
				TAttribute<FSlateColor>::CreateLambda([WeakSelf, ModeIdx]()
				{
					const bool bActive = WeakSelf.IsValid() && WeakSelf.Pin()->DisplayWindowModeIndex == ModeIdx;
					return FSlateColor(bActive
						? FLinearColor(0.88f, 0.62f, 0.16f, 1.0f)
						: FLinearColor(0.22f, 0.18f, 0.14f, 0.95f));
				}),
				FOnClicked::CreateLambda([WeakSelf, ModeIdx]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						Pinned->ApplyWindowMode(ModeIdx);
					}
					return FReply::Handled();
				})
			)
		];
	}

	// FPS cap pills (60, 120, 0=Unlimited)
	TSharedRef<SHorizontalBox> FpsCapPills = SNew(SHorizontalBox);
	const int32 FpsValues[3] = { 60, 120, 0 };
	const TCHAR* FpsEs[3] = { TEXT("60 FPS"), TEXT("120 FPS"), TEXT("ILIMITADO") };
	const TCHAR* FpsEn[3] = { TEXT("60 FPS"), TEXT("120 FPS"), TEXT("UNLIMITED") };
	for (int32 i = 0; i < 3; ++i)
	{
		const int32 CapVal = FpsValues[i];
		FpsCapPills->AddSlot()
		.FillWidth(1.0f)
		.Padding(i > 0 ? 6.0f : 0.0f, 0.0f, 0.0f, 0.0f)
		[
			MakePillButton(
				Tr(FpsEs[i], FpsEn[i]),
				TAttribute<FSlateColor>::CreateLambda([WeakSelf, CapVal]()
				{
					const bool bActive = WeakSelf.IsValid() && WeakSelf.Pin()->FrameRateCapValue == CapVal;
					return FSlateColor(bActive
						? FLinearColor(0.88f, 0.62f, 0.16f, 1.0f)
						: FLinearColor(0.22f, 0.18f, 0.14f, 0.95f));
				}),
				FOnClicked::CreateLambda([WeakSelf, CapVal]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						Pinned->ApplyFrameRateCap(CapVal);
					}
					return FReply::Handled();
				})
			)
		];
	}

	// Language switcher pills (Español / English)
	TSharedRef<SVerticalBox> LanguageControl = SNew(SVerticalBox)
	+ SVerticalBox::Slot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			MakePillButton(
				FText::FromString(TEXT("ESPAÑOL")),
				TAttribute<FSlateColor>::CreateLambda([WeakSelf]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						const bool bSelectedEs = Pinned->bPendingLanguageSwitch ? !Pinned->bTargetLanguageIsEnglish : !Pinned->bIsEnglish;
						return FSlateColor(bSelectedEs
							? FLinearColor(0.88f, 0.62f, 0.16f, 1.0f)
							: FLinearColor(0.22f, 0.18f, 0.14f, 0.95f));
					}
					return FSlateColor(FLinearColor(0.22f, 0.18f, 0.14f, 0.95f));
				}),
				FOnClicked::CreateLambda([WeakSelf]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						if (Pinned->bIsEnglish || (Pinned->bPendingLanguageSwitch && Pinned->bTargetLanguageIsEnglish))
						{
							Pinned->bTargetLanguageIsEnglish = false;
							Pinned->bPendingLanguageSwitch = true;
							Pinned->LanguageSwitchTimer = 0.0f;
						}
					}
					return FReply::Handled();
				})
			)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(8.0f, 0.0f, 0.0f, 0.0f)
		[
			MakePillButton(
				Tr(TEXT("INGLÉS"), TEXT("ENGLISH")),
				TAttribute<FSlateColor>::CreateLambda([WeakSelf]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						const bool bSelectedEn = Pinned->bPendingLanguageSwitch ? Pinned->bTargetLanguageIsEnglish : Pinned->bIsEnglish;
						return FSlateColor(bSelectedEn
							? FLinearColor(0.88f, 0.62f, 0.16f, 1.0f)
							: FLinearColor(0.22f, 0.18f, 0.14f, 0.95f));
					}
					return FSlateColor(FLinearColor(0.22f, 0.18f, 0.14f, 0.95f));
				}),
				FOnClicked::CreateLambda([WeakSelf]()
				{
					if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
					{
						if (!Pinned->bIsEnglish || (Pinned->bPendingLanguageSwitch && !Pinned->bTargetLanguageIsEnglish))
						{
							Pinned->bTargetLanguageIsEnglish = true;
							Pinned->bPendingLanguageSwitch = true;
							Pinned->LanguageSwitchTimer = 0.0f;
						}
					}
					return FReply::Handled();
				})
			)
		]
	]
	+ SVerticalBox::Slot()
	.AutoHeight()
	.Padding(0.0f, 6.0f, 0.0f, 0.0f)
	[
		SNew(STextBlock)
		.Visibility_Lambda([WeakSelf]()
		{
			return (WeakSelf.IsValid() && WeakSelf.Pin()->bPendingLanguageSwitch) ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.Text_Lambda([WeakSelf]()
		{
			if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
			{
				return Pinned->bTargetLanguageIsEnglish
					? FText::FromString(TEXT("⏳ Cambiando idioma a Inglés... / Switching to English..."))
					: FText::FromString(TEXT("⏳ Switching language to Spanish... / Cambiando a Español..."));
			}
			return FText::GetEmpty();
		})
		.Font(FCoreStyle::GetDefaultFontStyle("Italic", 10))
		.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.35f, 0.95f)))
	];

	return SNew(SScaleBox)
	.Stretch(EStretch::ScaleToFit)
	[
		SNew(SBox)
		.WidthOverride(1920.0f)
		.HeightOverride(1080.0f)
		[
			SNew(SOverlay)

			// 1. Key art background + soft atmospheric blur & dark espresso tint
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Image(BrushMainMenuBg.IsValid() ? BrushMainMenuBg.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBackgroundBlur)
				.BlurStrength(14.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.035f, 0.028f, 0.022f, 0.80f))
					.Padding(0.0f)
				]
			]

			// 2. Top wooden banner header (cohesive with Character Select)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 44.0f, 0.0f, 0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(520.0f)
					.HeightOverride(88.0f)
					[
						SNew(SOverlay)
						+ SOverlay::Slot()
						.HAlign(HAlign_Fill)
						.VAlign(VAlign_Fill)
						[
							SNew(SImage)
							.Image(BrushTitleBanner.IsValid() ? BrushTitleBanner.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
							.ColorAndOpacity(BrushTitleBanner.IsValid() ? FLinearColor::White : FLinearColor(0.28f, 0.17f, 0.08f, 0.96f))
						]
						+ SOverlay::Slot()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Tr(TEXT("OPCIONES Y AJUSTES"), TEXT("OPTIONS & SETTINGS")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 24))
							.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.90f, 0.42f, 1.0f)))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
							.ShadowOffset(FVector2D(2.0f, 2.0f))
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(Tr(TEXT("Personaliza audio, video, idioma y consulta tus controles de combate"), TEXT("Customize audio, video, language and review your combat controls")))
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.86f, 0.74f, 0.92f)))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
				]
			]

			// 3. Main dual-column bronze-framed Options Card
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 40.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(1180.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.86f, 0.62f, 0.16f, 0.96f))
					.Padding(FMargin(2.5f))
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.075f, 0.058f, 0.042f, 0.95f))
						.Padding(FMargin(32.0f, 26.0f))
						[
							SNew(SVerticalBox)

							// Two-column settings body
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)

								// LEFT COLUMN: AUDIO & MIX
								+ SHorizontalBox::Slot()
								.FillWidth(0.49f)
								.VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										MakeSectionHeader(Tr(TEXT("✦  AUDIO Y MEZCLA"), TEXT("✦  AUDIO & MIX")))
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 12.0f)
									[
										MakeVolumeRow(Tr(TEXT("Volumen Maestro"), TEXT("Master Volume")), &SPanFightersMasterWidget::MasterVolume, true)
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 12.0f)
									[
										MakeVolumeRow(Tr(TEXT("Volumen de Música"), TEXT("Music Volume")), &SPanFightersMasterWidget::MusicVolume, false)
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 16.0f)
									[
										MakeVolumeRow(Tr(TEXT("Efectos Sonoros (SFX)"), TEXT("Sound Effects (SFX)")), &SPanFightersMasterWidget::SFXVolume, false)
									]

									// Audio Preview & Status Row
									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										SNew(SHorizontalBox)

										+ SHorizontalBox::Slot()
										.AutoWidth()
										.VAlign(VAlign_Center)
										[
											MakePillButton(
												Tr(TEXT("▶  PROBAR AUDIO SFX"), TEXT("▶  TEST SFX AUDIO")),
												FSlateColor(FLinearColor(0.68f, 0.44f, 0.14f, 1.0f)),
												FOnClicked::CreateLambda([WeakSelf]()
												{
													if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
													{
														if (Pinned->OwnerPC.IsValid())
														{
															if (USoundBase* BellSfx = LoadObject<USoundBase>(nullptr, TEXT("/Game/Audio/SFX/SFX_BoxingBell.SFX_BoxingBell")))
															{
																Pinned->OwnerPC->PlaySFX2D(BellSfx, 1.0f, 1.0f);
															}
														}
													}
													return FReply::Handled();
												}),
												210.0f
											)
										]

										+ SHorizontalBox::Slot()
										.FillWidth(1.0f)
										.VAlign(VAlign_Center)
										.Padding(14.0f, 0.0f, 0.0f, 0.0f)
										[
											SNew(STextBlock)
											.Text(Tr(TEXT("Guardado automático al ajustar"), TEXT("Auto-saved on change")))
											.Font(FCoreStyle::GetDefaultFontStyle("Italic", 11))
											.ColorAndOpacity(FSlateColor(FLinearColor(0.74f, 0.68f, 0.56f, 0.90f)))
										]
									]
								]

								// Vertical bronze separator
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(26.0f, 4.0f)
								[
									SNew(SBox)
									.WidthOverride(1.5f)
									[
										SNew(SBorder)
										.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
										.BorderBackgroundColor(FLinearColor(0.86f, 0.62f, 0.16f, 0.32f))
									]
								]

								// RIGHT COLUMN: DISPLAY, QUALITY & LANGUAGE
								+ SHorizontalBox::Slot()
								.FillWidth(0.51f)
								.VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)

									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										MakeSectionHeader(Tr(TEXT("✦  PANTALLA, CALIDAD E IDIOMA"), TEXT("✦  DISPLAY, QUALITY & LANGUAGE")))
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 12.0f)
									[
										MakeLabeledSettingRow(Tr(TEXT("Calidad Gráfica (UE5 Scalability)"), TEXT("Graphics Quality (UE5 Scalability)")), QualityPills)
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 12.0f)
									[
										MakeLabeledSettingRow(Tr(TEXT("Modo de Pantalla"), TEXT("Window Mode")), WindowModePills)
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									.Padding(0.0f, 0.0f, 0.0f, 12.0f)
									[
										MakeLabeledSettingRow(Tr(TEXT("Límite de FPS"), TEXT("Frame Rate Cap")), FpsCapPills)
									]

									+ SVerticalBox::Slot()
									.AutoHeight()
									[
										MakeLabeledSettingRow(Tr(TEXT("Idioma de la Interfaz"), TEXT("Interface Language")), LanguageControl)
									]
								]
							]

							// Bottom Footer Divider inside Settings Card
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 22.0f, 0.0f, 14.0f)
							[
								SNew(SBox)
								.HeightOverride(1.5f)
								[
									SNew(SBorder)
									.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
									.BorderBackgroundColor(FLinearColor(0.86f, 0.62f, 0.16f, 0.28f))
								]
							]

							// Bottom Strip: Quick Controls Reference + Reset Defaults
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.VAlign(VAlign_Center)
								[
									SNew(SBorder)
									.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
									.BorderBackgroundColor(FLinearColor(0.11f, 0.085f, 0.06f, 0.90f))
									.Padding(FMargin(14.0f, 8.0f))
									[
										SNew(STextBlock)
										.Text(Tr(
											TEXT("CONTROLES:   [WASD] Mover   •   [Clic Izq] Jab   •   [Clic Der] Gancho   •   [Espacio] Saltar   •   [ESC / P] Pausa"),
											TEXT("CONTROLS:   [WASD] Move   •   [Left Click] Jab   •   [Right Click] Hook   •   [Space] Jump   •   [ESC / P] Pause")
										))
										.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
										.ColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.85f, 0.68f, 0.95f)))
									]
								]

								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(16.0f, 0.0f, 0.0f, 0.0f)
								[
									MakePillButton(
										Tr(TEXT("RESTABLECER"), TEXT("RESET DEFAULTS")),
										FSlateColor(FLinearColor(0.55f, 0.22f, 0.16f, 0.98f)),
										FOnClicked::CreateLambda([WeakSelf]()
										{
											if (TSharedPtr<SPanFightersMasterWidget> Pinned = WeakSelf.Pin())
											{
												Pinned->ResetOptionsToDefaults();
											}
											return FReply::Handled();
										}),
										170.0f
									)
								]
							]
						]
					]
				]
			]

			// 4. Bottom-left Rustic Wooden Back Button (matching Character Select & Winner screen)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(42.0f, 0.0f, 0.0f, 32.0f)
			[
				BuildWoodenNavButton(
					Tr(TEXT("« MENÚ PRINCIPAL"), TEXT("« MAIN MENU")),
					FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked),
					240.0f
				)
			]
		]
	];
}

// --------------------------------------------------------------------------
// CHARACTER SELECT HELPERS & 3D ARCADE FIGHTER GRID (Matching reference image)
// --------------------------------------------------------------------------
FText SPanFightersMasterWidget::GetSelectedCharacterCardName() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloChocolate:
			return Tr(TEXT("POLLO CHOCOLATE"), TEXT("CHOCOLATE ROOSTER"));
		case EPanFightersCharacter::GalloGato:
			return Tr(TEXT("GALLO GATO"), TEXT("CAT ROOSTER"));
		default:
			break;
		}
	}
	return Tr(TEXT("POLLO BLANCO"), TEXT("WHITE ROOSTER"));
}

FText SPanFightersMasterWidget::GetSelectedCharacterCardSubtitle() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloChocolate:
			return Tr(TEXT("El Retador Feroz"), TEXT("The Fierce Challenger"));
		case EPanFightersCharacter::GalloGato:
			return Tr(TEXT("El Minino Boxeador"), TEXT("The Boxing Feline"));
		default:
			break;
		}
	}
	return Tr(TEXT("El Campeon Blanco"), TEXT("The White Champion"));
}

FText SPanFightersMasterWidget::GetSelectedCharacterCardDescription() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloChocolate:
			return Tr(TEXT("Fuerza y Pegada Brutal"), TEXT("Brutal Strength & Power"));
		case EPanFightersCharacter::GalloGato:
			return Tr(TEXT("Astuto, Ágil y Letal"), TEXT("Clever, Agile & Lethal"));
		default:
			break;
		}
	}
	return Tr(TEXT("Equilibrado y Veloz"), TEXT("Balanced & Fast"));
}

const FSlateBrush* SPanFightersMasterWidget::GetSelectedCharacterCardAvatar() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloChocolate:
			return BrushPolloChocolate.IsValid() ? BrushPolloChocolate.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		case EPanFightersCharacter::GalloGato:
			return BrushGalloGato.IsValid() ? BrushGalloGato.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		default:
			break;
		}
	}
	return BrushPolloBlanco.IsValid() ? BrushPolloBlanco.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
}

FSlateColor SPanFightersMasterWidget::GetSelectedSlotBorderColor(EPanFightersCharacter SlotChar) const
{
	const EPanFightersCharacter CurrentChar = OwnerPC.IsValid() ? OwnerPC->GetSelectedCharacter() : EPanFightersCharacter::PolloBlanco;
	if (CurrentChar == SlotChar)
	{
		return FSlateColor(FLinearColor(1.0f, 0.88f, 0.22f, 1.0f));
	}
	return FSlateColor(FLinearColor(0.22f, 0.17f, 0.12f, 0.85f));
}

FReply SPanFightersMasterWidget::OnCharacterSlotClicked(EPanFightersCharacter InChar)
{
	if (OwnerPC.IsValid())
	{
		if (OwnerPC->GetSelectedCharacter() == InChar)
		{
			OwnerPC->ConfirmCharacterAndProceed();
		}
		else
		{
			OwnerPC->SetPreviewCharacter(InChar);
		}
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnConfirmCharacterClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ConfirmCharacterAndProceed();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::StageSelect);
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SPanFightersMasterWidget::BuildCharacterInfoCard()
{
	return SNew(SBox)
		.WidthOverride(290.0f)
		.HeightOverride(115.0f)
		[
			SNew(SOverlay)

			// 1. Rustic rounded dark wood panel with golden border and corner rivets
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Image(BrushCardBg.IsValid() ? BrushCardBg.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
				.ColorAndOpacity(BrushCardBg.IsValid() ? FLinearColor::White : FLinearColor(0.14f, 0.10f, 0.07f, 0.95f))
			]

			// 2. Inner Content
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(18.0f, 15.0f, 16.0f, 15.0f)
			[
				SNew(SHorizontalBox)

				// Square Avatar Frame on Left (no '1P', no ribbons)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 14.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(64.0f)
					.HeightOverride(64.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.85f, 0.68f, 0.25f, 1.0f))
						.Padding(FMargin(2.0f))
						[
							SNew(SImage)
							.Image(this, &SPanFightersMasterWidget::GetSelectedCharacterCardAvatar)
						]
					]
				]

				// Character Details on Right
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)

					// Name in BOLD WHITE thick font
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 3.0f)
					[
						SNew(STextBlock)
						.Text(this, &SPanFightersMasterWidget::GetSelectedCharacterCardName)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
					]

					// Title in finer font
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 2.0f)
					[
						SNew(STextBlock)
						.Text(this, &SPanFightersMasterWidget::GetSelectedCharacterCardSubtitle)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.85f, 0.78f, 0.95f)))
					]

					// Attributes in finer font
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SPanFightersMasterWidget::GetSelectedCharacterCardDescription)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.75f, 0.68f, 0.85f)))
					]
				]
			]
		];
}

TSharedRef<SWidget> SPanFightersMasterWidget::BuildCharacterGridSlot(
	EPanFightersCharacter CharId,
	const FText& SlotName,
	TSharedPtr<FSlateBrush> AvatarBrush,
	bool bIsAvailable)
{
	const float SlotSize = 118.0f;

	if (!bIsAvailable)
	{
		// Locked slots: 60% opacity dark semi-transparent tile with "PRÓXIMAMENTE" centered in clean subtle uppercase. No padlocks!
		return SNew(SBox)
			.WidthOverride(SlotSize)
			.HeightOverride(SlotSize)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.08f, 0.06f, 0.05f, 0.55f))
				.Padding(FMargin(1.5f))
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.16f, 0.13f, 0.10f, 0.60f))
					.Padding(FMargin(4.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Tr(TEXT("PRÓXIMAMENTE"), TEXT("COMING SOON")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.60f, 0.55f, 0.50f, 0.55f)))
					]
				]
			];
	}

	// Active slots (POLLO BLANCO & POLLO CHOCOLATE): 3D portrait, clean name tag at top, glowing yellow/golden highlight when selected
	return SNew(SBox)
		.WidthOverride(SlotSize)
		.HeightOverride(SlotSize)
		[
			SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ContentPadding(FMargin(0.0f))
			.OnClicked(FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnCharacterSlotClicked, CharId))
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor_Raw(this, &SPanFightersMasterWidget::GetSelectedSlotBorderColor, CharId)
				.Padding(FMargin(3.0f))
				[
					SNew(SOverlay)

					// 1. 3D Character Portrait
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						AvatarBrush.IsValid()
						? SNew(SImage).Image(AvatarBrush.Get())
						: SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.12f, 0.10f, 1.0f)))
					]

					// 2. Clean Top Name Tag (ONLY the character name)
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Top)
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.06f, 0.04f, 0.03f, 0.88f))
						.Padding(FMargin(8.0f, 2.5f))
						[
							SNew(STextBlock)
							.Text(SlotName)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
							.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.90f, 1.0f)))
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SPanFightersMasterWidget::BuildCharacterSelectMenu()
{
	return SNew(SScaleBox)
	.Stretch(EStretch::ScaleToFit)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(1920.0f)
		.HeightOverride(1080.0f)
		[
			SNew(SOverlay)

			// TOP TITLE BANNER (Rustic dark wood panel with bronze border, rivets, and warm golden stylized text)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 24.0f, 0.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(500.0f)
				.HeightOverride(90.0f)
				[
					SNew(SImage)
					.Image(BrushTitleBanner.IsValid() ? BrushTitleBanner.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
				]
			]

			// TOP-LEFT CARD: Character Information Card (Matches user black reference box #2)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Top)
			.Padding(42.0f, 38.0f, 0.0f, 0.0f)
			[
				BuildCharacterInfoCard()
			]

			// RIGHT SIDE: Elegant 3x3 Fighter Grid Board (Matches user black reference box #3)
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Top)
			.Padding(0.0f, 215.0f, 85.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(445.0f)
				.HeightOverride(430.0f)
				[
					SNew(SOverlay)

					// Board Background Texture
					+ SOverlay::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					[
						SNew(SImage)
						.Image(BrushGridBg.IsValid() ? BrushGridBg.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
					]

					// 3x3 Slots Inner Container
					+ SOverlay::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(20.0f, 20.0f, 20.0f, 20.0f)
					[
						SNew(SVerticalBox)

						// Row 1 (Pollo Blanco, Pollo Chocolate, Próximamente)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								BuildCharacterGridSlot(EPanFightersCharacter::PolloBlanco, Tr(TEXT("POLLO BLANCO"), TEXT("WHITE ROOSTER")), BrushPolloBlanco, true)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								BuildCharacterGridSlot(EPanFightersCharacter::PolloChocolate, Tr(TEXT("POLLO CHOCOLATE"), TEXT("CHOCOLATE ROOSTER")), BrushPolloChocolate, true)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								BuildCharacterGridSlot(EPanFightersCharacter::GalloGato, Tr(TEXT("GALLO GATO"), TEXT("CAT ROOSTER")), BrushGalloGato, true)
							]
						]

						// Row 2 (Próximamente x3)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 10.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								BuildCharacterGridSlot(EPanFightersCharacter::None, FText::GetEmpty(), nullptr, false)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								BuildCharacterGridSlot(EPanFightersCharacter::None, FText::GetEmpty(), nullptr, false)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								BuildCharacterGridSlot(EPanFightersCharacter::None, FText::GetEmpty(), nullptr, false)
							]
						]

						// Row 3 (Próximamente x3)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								BuildCharacterGridSlot(EPanFightersCharacter::None, FText::GetEmpty(), nullptr, false)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(0.0f, 0.0f, 10.0f, 0.0f)
							[
								BuildCharacterGridSlot(EPanFightersCharacter::None, FText::GetEmpty(), nullptr, false)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								BuildCharacterGridSlot(EPanFightersCharacter::None, FText::GetEmpty(), nullptr, false)
							]
						]
					]
				]
			]

			// BOTTOM CORNER NAVIGATION BUTTONS (Rustic wood panel with bronze border and rivets)
			// Bottom-Left Button: MENÚ PRINCIPAL
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(42.0f, 0.0f, 0.0f, 32.0f)
			[
				BuildWoodenNavButton(
					Tr(TEXT("« MENÚ PRINCIPAL"), TEXT("« MAIN MENU")),
					FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked),
					230.0f
				)
			]

			// Bottom-Right Button: ELEGIR ESCENARIO
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f, 0.0f, 85.0f, 32.0f)
			[
				BuildWoodenNavButton(
					Tr(TEXT("ELEGIR ESCENARIO »"), TEXT("SELECT STAGE »")),
					FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnConfirmCharacterClicked),
					230.0f
				)
			]
		]
	];
}

// --------------------------------------------------------------------------
// STAGE SELECT MENU BUILDER (Con imágenes reales de cada mapa)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildStageSelectMenu()
{
	return SNew(SOverlay)

	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)

		// Main Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 0.0f, 0.0f, 22.0f)
		[
			SNew(STextBlock)
			.Text(Tr(TEXT("SELECCIÓN DE ESCENARIO"), TEXT("STAGE SELECTION")))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 36))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.35f, 1.0f)))
		]

		// 3 Scenario Cards in Horizontal Box
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)

			// ---------------- ESCENARIO 1: EL RANCHITO ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildStageCard(
					EPanFightersStage::Stage1_ElRanchito,
					Tr(TEXT("El Ranchito"), TEXT("The Little Ranch")),
					Tr(TEXT("El Hogar del Gallo"), TEXT("The Rooster's Home")),
					FLinearColor(0.95f, 0.70f, 0.15f, 1.0f),
					BrushRanchito
				)
			]

			// ---------------- ESCENARIO 2: EL CAMPO LLANO ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildStageCard(
					EPanFightersStage::Stage2_ElCampoLlano,
					Tr(TEXT("El Campo Llano"), TEXT("The Open Field")),
					Tr(TEXT("La Sabana Campesina"), TEXT("The Countryside Savanna")),
					FLinearColor(0.38f, 0.75f, 0.32f, 1.0f),
					BrushCampoLlano
				)
			]

			// ---------------- ESCENARIO 3: EL SARTÉN ----------------
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(12.0f, 0.0f)
			[
				BuildStageCard(
					EPanFightersStage::Stage3_ElSarten,
					Tr(TEXT("El Sartén"), TEXT("The Frying Pan")),
					Tr(TEXT("El Fuego Culinario"), TEXT("The Culinary Fire")),
					FLinearColor(0.92f, 0.38f, 0.14f, 1.0f),
					BrushSarten
				)
			]
		]

		// Navigation Buttons (Cambiar Personaje y Volver al Menú Principal)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 22.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(260.0f)
				.HeightOverride(42.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.65f, 0.30f, 1.0f)))
					.OnClicked(this, &SPanFightersMasterWidget::OnChangeCharacterClicked)
					.ContentPadding(FMargin(16.0f, 6.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Tr(TEXT("« CAMBIAR PERSONAJE"), TEXT("« CHANGE CHARACTER")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(260.0f)
				.HeightOverride(42.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.55f, 0.20f, 1.0f)))
					.OnClicked(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked)
					.ContentPadding(FMargin(16.0f, 6.0f))
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Tr(TEXT("« VOLVER AL MENÚ PRINCIPAL"), TEXT("« BACK TO MAIN MENU")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// PAUSE MENU BUILDER (Difumina un 40% el fondo para resaltar el menú)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildInGameMenu()
{
	return SNew(SOverlay)

	// 1. Fondo difuminado + 40% oscurecimiento (SBackgroundBlur + Border a 0.40 alpha)
	+ SOverlay::Slot()
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBackgroundBlur)
		.BlurStrength(22.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.40f)) // Difumina y oscurece un 40% el fondo
			.Padding(0.0f)
		]
	]

	// 2. Diálogo central del Menú de Pausa
	+ SOverlay::Slot()
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(440.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.09f, 0.08f, 0.07f, 0.98f))
			.Padding(FMargin(32.0f, 26.0f))
			[
				SNew(SVerticalBox)

				// Header badge
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 18.0f)
				[
					SNew(STextBlock)
					.Text(Tr(TEXT("✦   J U E G O   E N   P A U S A   ✦"), TEXT("✦   G A M E   P A U S E D   ✦")))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.40f, 1.0f)))
				]

				// BUTTON 1: CONTINUAR JUEGO (Reanudar)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.68f, 0.15f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnResumeInGameClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Tr(TEXT("CONTINUAR JUEGO"), TEXT("RESUME GAME")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// BUTTON 2: CAMBIAR DE ESCENARIO
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.70f, 0.30f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnChangeStageFromInGameClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Tr(TEXT("SELECCIONAR OTRO ESCENARIO"), TEXT("SELECT ANOTHER STAGE")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// BUTTON 3: MENÚ DE INICIO
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.52f, 0.22f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Tr(TEXT("MENÚ DE INICIO"), TEXT("MAIN MENU")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]

				// BUTTON 4: SALIR DEL JUEGO
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(46.0f)
					[
						SNew(SButton)
						.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
						.ButtonColorAndOpacity(FSlateColor(FLinearColor(0.70f, 0.22f, 0.18f, 1.0f)))
						.OnClicked(this, &SPanFightersMasterWidget::OnQuitClicked)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(Tr(TEXT("SALIR DEL JUEGO"), TEXT("QUIT GAME")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
							.ColorAndOpacity(FSlateColor(FLinearColor::White))
						]
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// WOODEN CORNER NAVIGATION BUTTON (Shared between Character Select & Winner Screen)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildWoodenNavButton(const FText& ButtonText, FOnClicked OnClicked, float ButtonWidth)
{
	return SNew(SBox)
	.WidthOverride(ButtonWidth)
	.HeightOverride(52.0f)
	[
		SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(FMargin(0.0f))
		.OnClicked(OnClicked)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Image(BrushNavButton.IsValid() ? BrushNavButton.Get() : FCoreStyle::Get().GetBrush("WhiteBrush"))
				.ColorAndOpacity(BrushNavButton.IsValid() ? FLinearColor::White : FLinearColor(0.18f, 0.12f, 0.08f, 0.95f))
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(ButtonText)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.92f, 0.70f, 1.0f)))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.85f))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			]
		]
	];
}

// --------------------------------------------------------------------------
// MATCH RESULT / WINNER SHOWCASE OVERLAY (3D Full-Body Winner + Bottom Sign + 2 Corner Wooden Buttons)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildMatchResultMenu(bool bVictory)
{
	const FText WinnerName = OwnerPC.IsValid() ? OwnerPC->GetWinnerCharacterName() : Tr(TEXT("POLLO BLANCO"), TEXT("WHITE ROOSTER"));
	const int32 PlayerWins = OwnerPC.IsValid() ? OwnerPC->GetPlayerRoundWins() : 0;
	const int32 RivalWins = OwnerPC.IsValid() ? OwnerPC->GetRivalRoundWins() : 0;
	const bool bByRounds = OwnerPC.IsValid() && OwnerPC->WasMatchCompletedByRounds();

	const FText ChampionSubline = bByRounds
		? FText::FromString(FString::Printf(TEXT("%s   •   %s %d - %d"), *WinnerName.ToString(), *Tr(TEXT("MARCADOR FINAL"), TEXT("FINAL SCORE")).ToString(), PlayerWins, RivalWins))
		: FText::FromString(FString::Printf(TEXT("%s   •   %s"), *WinnerName.ToString(), *Tr(TEXT("CAMPEÓN DEL RUEDO"), TEXT("RING CHAMPION")).ToString()));

	const FLinearColor OuterBronzeColor = bVictory ? FLinearColor(0.86f, 0.62f, 0.16f, 0.98f) : FLinearColor(0.75f, 0.30f, 0.18f, 0.98f);
	const FLinearColor WinnerWordColor = bVictory ? FLinearColor(1.0f, 0.86f, 0.24f, 1.0f) : FLinearColor(1.0f, 0.48f, 0.30f, 1.0f);

	return SNew(SScaleBox)
	.Stretch(EStretch::ScaleToFit)
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(1920.0f)
		.HeightOverride(1080.0f)
		[
			SNew(SOverlay)

			// Letrero "WINNER" ubicado debajo del gallo 3D en cuerpo completo (sin tapar al personaje)
			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f, 0.0f, 0.0f, 76.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(OuterBronzeColor)
				.Padding(FMargin(3.0f))
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.11f, 0.07f, 0.04f, 0.94f))
					.Padding(FMargin(44.0f, 12.0f))
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(TEXT("WINNER")))
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 38))
							.ColorAndOpacity(FSlateColor(WinnerWordColor))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
							.ShadowOffset(FVector2D(2.0f, 2.0f))
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0f, 2.0f, 0.0f, 0.0f)
						[
							SNew(STextBlock)
							.Text(ChampionSubline)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.88f, 0.72f, 0.95f)))
							.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
							.ShadowOffset(FVector2D(1.0f, 1.0f))
						]
					]
				]
			]

			// Botón inferior izquierdo: Reintentar (mismo estilo de madera que Selección de Personaje)
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Bottom)
			.Padding(42.0f, 0.0f, 0.0f, 32.0f)
			[
				BuildWoodenNavButton(
					Tr(TEXT("« REINTENTAR"), TEXT("« RETRY")),
					FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnRetryClicked),
					240.0f
				)
			]

			// Botón inferior derecho: Volver al Menú Principal
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Bottom)
			.Padding(0.0f, 0.0f, 85.0f, 32.0f)
			[
				BuildWoodenNavButton(
					Tr(TEXT("VOLVER AL MENÚ PRINCIPAL »"), TEXT("BACK TO MAIN MENU »")),
					FOnClicked::CreateSP(this, &SPanFightersMasterWidget::OnBackToMainMenuClicked),
					295.0f
				)
			]
		]
	];
}

// --------------------------------------------------------------------------
// HELPER: BUILD STAGE CARD (Con imagen real del mapa)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildStageCard(
	EPanFightersStage StageId,
	const FText& StageTitle,
	const FText& Subtitle,
	const FLinearColor& AccentColor,
	TSharedPtr<FSlateBrush> StageImageBrush)
{
	return SNew(SBox)
	.WidthOverride(300.0f)
	.HeightOverride(310.0f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.09f, 0.08f, 0.07f, 0.96f))
		.Padding(FMargin(14.0f, 14.0f))
		[
			SNew(SVerticalBox)

			// Stage Title
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(StageTitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.90f, 1.0f)))
			]

			// Subtitle
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(Subtitle)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FSlateColor(AccentColor))
			]

			// Image Preview Container (270x135)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(SBox)
				.WidthOverride(270.0f)
				.HeightOverride(135.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.12f, 0.10f, 0.08f, 1.0f))
					.Padding(FMargin(1.5f))
					[
						StageImageBrush.IsValid()
						? SNew(SImage).Image(StageImageBrush.Get())
						: SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
					]
				]
			]

			// Action Button: Entrar a Jugar en Primera Persona
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			[
				SNew(SBox)
				.HeightOverride(40.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
					.ButtonColorAndOpacity(FSlateColor(AccentColor))
					.OnClicked_Lambda([this, StageId]() { return OnStageClicked(StageId); })
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Tr(TEXT("ENTRAR A JUGAR"), TEXT("ENTER MATCH")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
						.ColorAndOpacity(FSlateColor(FLinearColor::White))
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// HELPER: BUILD MENU BUTTON
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildMenuButton(
	const FText& ButtonText,
	FOnClicked OnClicked,
	const FLinearColor& AccentColor)
{
	return SNew(SBox)
	.WidthOverride(320.0f)
	.HeightOverride(48.0f)
	[
		SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
		.ButtonColorAndOpacity(FSlateColor(AccentColor))
		.OnClicked(OnClicked)
		.ContentPadding(FMargin(16.0f, 8.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(ButtonText)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			.ColorAndOpacity(FSlateColor(FLinearColor::White))
		]
	];
}

// --------------------------------------------------------------------------
// CALLBACKS
// --------------------------------------------------------------------------
FReply SPanFightersMasterWidget::OnPlayClicked()
{
	SetScreenState(EPanFightersScreenState::CharacterSelect);
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnCharacterClicked(EPanFightersCharacter CharacterId)
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->SelectCharacter(CharacterId);
	}
	else
	{
		SetScreenState(EPanFightersScreenState::StageSelect);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnChangeCharacterClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ReturnToCharacterSelect();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::CharacterSelect);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnRetryClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->RetryMatch();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::InGame);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnOptionsClicked()
{
	SetScreenState(EPanFightersScreenState::OptionsMenu);
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnQuitClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->QuitGameAction();
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnBackToMainMenuClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ReturnToMainMenu();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::MainMenu);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnStageClicked(EPanFightersStage StageId)
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->SelectStage(StageId);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnResumeInGameClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ResumeGame();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::InGame);
	}
	return FReply::Handled();
}

FReply SPanFightersMasterWidget::OnChangeStageFromInGameClicked()
{
	if (OwnerPC.IsValid())
	{
		OwnerPC->ReturnToStageSelect();
	}
	else
	{
		SetScreenState(EPanFightersScreenState::StageSelect);
	}
	return FReply::Handled();
}

// --------------------------------------------------------------------------
// PROGRESS & TEXT HELPERS
// --------------------------------------------------------------------------
TOptional<float> SPanFightersMasterWidget::GetProgressBarPercent() const
{
	return SplashProgress;
}

FText SPanFightersMasterWidget::GetProgressPercentText() const
{
	const int32 Percent = FMath::Clamp(FMath::RoundToInt(SplashProgress * 100.0f), 0, 100);
	return FText::Format(Tr(TEXT("Cargando: {0}%"), TEXT("Loading: {0}%")), FText::AsNumber(Percent));
}

FText SPanFightersMasterWidget::GetLoadingStatusText() const
{
	if (SplashProgress < 0.25f)
	{
		return Tr(TEXT("Iniciando motor Pan Fighters..."), TEXT("Initializing Pan Fighters engine..."));
	}
	else if (SplashProgress < 0.55f)
	{
		return Tr(TEXT("Cargando escenarios campesinos..."), TEXT("Loading countryside stages..."));
	}
	else if (SplashProgress < 0.85f)
	{
		return Tr(TEXT("Configurando texturas e iluminación..."), TEXT("Configuring textures and lighting..."));
	}
	else if (SplashProgress < 0.99f)
	{
		return Tr(TEXT("Sincronizando controles..."), TEXT("Synchronizing controls..."));
	}
	else
	{
		return Tr(TEXT("¡Listo para jugar!"), TEXT("Ready to play!"));
	}
}

// --------------------------------------------------------------------------
// IN-GAME COMBAT HUD BUILDER (Health Bars, Avatars, VS Emblem)
// --------------------------------------------------------------------------
TSharedRef<SWidget> SPanFightersMasterWidget::BuildInGameHUD()
{
	return BuildInGameFightHUD();
}

TSharedRef<SWidget> SPanFightersMasterWidget::BuildInGameFightHUD()
{
	return SNew(SBox)
	.WidthOverride(1060.0f)
	.HeightOverride(70.0f)
	[
		SNew(SHorizontalBox)

		// ---------------- PLAYER FIGHTER (LEFT SIDE) ----------------
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			// Player Avatar Frame
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(56.0f)
				.HeightOverride(56.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.95f, 0.75f, 0.15f, 1.0f))
					.Padding(FMargin(2.5f))
					[
						SNew(SImage)
						.Image(this, &SPanFightersMasterWidget::GetPlayerAvatarBrush)
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(10.0f, 1.0f))
			]

			// Name & Health Bar Column
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				// Player Name
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(this, &SPanFightersMasterWidget::GetPlayerNameText)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.88f, 0.35f, 1.0f)))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
				]

				// Player Health Bar Frame
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(24.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.98f))
						.Padding(FMargin(2.0f))
						[
							SNew(SOverlay)

							// Background track
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(FLinearColor(0.12f, 0.07f, 0.07f, 0.95f))
								.Padding(0.0f)
							]

							// Ghost damage bar (amber)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetPlayerGhostHealthPercent)
								.FillColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.50f, 0.15f, 0.85f)))
								.BorderPadding(FVector2D::ZeroVector)
							]

							// Active health bar (green -> amber -> red)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetPlayerHealthPercent)
								.FillColorAndOpacity(this, &SPanFightersMasterWidget::GetPlayerHealthBarColor)
								.BorderPadding(FVector2D::ZeroVector)
							]

							// HP Text readout
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SPanFightersMasterWidget::GetPlayerHealthText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.98f, 0.95f, 1.0f)))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
								.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
							]
						]
					]
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SSpacer)
			.Size(FVector2D(16.0f, 1.0f))
		]

		// ---------------- CENTER VS & ROUND BADGE ----------------
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(104.0f)
			.HeightOverride(66.0f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.96f))
				.Padding(FMargin(6.0f, 4.0f))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SPanFightersMasterWidget::GetRoundText)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.85f, 0.55f, 0.95f)))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(FText::FromString(TEXT("VS")))
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
						.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.82f, 0.15f, 1.0f)))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SPanFightersMasterWidget::GetRoundScoreText)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.72f, 0.25f, 0.98f)))
					]
				]
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SSpacer)
			.Size(FVector2D(16.0f, 1.0f))
		]

		// ---------------- RIVAL / NPC FIGHTER (RIGHT SIDE) ----------------
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SHorizontalBox)

			// Name & Health Bar Column
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)

				// Rival Name
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(0.0f, 0.0f, 0.0f, 4.0f)
				[
					SNew(STextBlock)
					.Text(this, &SPanFightersMasterWidget::GetRivalNameText)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.48f, 0.25f, 1.0f)))
					.ShadowOffset(FVector2D(1.0f, 1.0f))
					.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.90f))
				]

				// Rival Health Bar Frame
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(24.0f)
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.08f, 0.07f, 0.06f, 0.98f))
						.Padding(FMargin(2.0f))
						[
							SNew(SOverlay)

							// Background track
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SBorder)
								.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
								.BorderBackgroundColor(FLinearColor(0.12f, 0.07f, 0.07f, 0.95f))
								.Padding(0.0f)
							]

							// Ghost damage bar (amber)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetRivalGhostHealthPercent)
								.FillColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.50f, 0.15f, 0.85f)))
								.BorderPadding(FVector2D::ZeroVector)
							]

							// Active health bar (green -> amber -> red)
							+ SOverlay::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								SNew(SProgressBar)
								.Percent(this, &SPanFightersMasterWidget::GetRivalHealthPercent)
								.FillColorAndOpacity(this, &SPanFightersMasterWidget::GetRivalHealthBarColor)
								.BorderPadding(FVector2D::ZeroVector)
							]

							// HP Text readout
							+ SOverlay::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(this, &SPanFightersMasterWidget::GetRivalHealthText)
								.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
								.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.98f, 0.95f, 1.0f)))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
								.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f))
							]
						]
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
				.Size(FVector2D(10.0f, 1.0f))
			]

			// Rival Avatar Frame
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(56.0f)
				.HeightOverride(56.0f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor(0.85f, 0.35f, 0.20f, 1.0f))
					.Padding(FMargin(2.5f))
					[
						SNew(SImage)
						.Image(this, &SPanFightersMasterWidget::GetRivalAvatarBrush)
					]
				]
			]
		]
	];
}

// --------------------------------------------------------------------------
// COMBAT HUD & HIT REACTION HELPERS
// --------------------------------------------------------------------------
EVisibility SPanFightersMasterWidget::GetInGameFightHUDVisibility() const
{
	return (CurrentState == EPanFightersScreenState::InGame) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

TOptional<float> SPanFightersMasterWidget::GetPlayerHealthPercent() const
{
	if (OwnerPC.IsValid())
	{
		return FMath::Clamp(OwnerPC->GetPlayerCurrentHealth() / 100.0f, 0.0f, 1.0f);
	}
	return 1.0f;
}

TOptional<float> SPanFightersMasterWidget::GetPlayerGhostHealthPercent() const
{
	return FMath::Clamp(PlayerGhostHealth / 100.0f, 0.0f, 1.0f);
}

FSlateColor SPanFightersMasterWidget::GetPlayerHealthBarColor() const
{
	const float Ratio = GetPlayerHealthPercent().Get(1.0f);
	return ComputeHealthColor(Ratio);
}

FText SPanFightersMasterWidget::GetPlayerHealthText() const
{
	const int32 HP = FMath::Clamp(FMath::RoundToInt(OwnerPC.IsValid() ? OwnerPC->GetPlayerCurrentHealth() : 100.0f), 0, 100);
	return FText::Format(FText::FromString(TEXT("{0} / 100 HP")), FText::AsNumber(HP));
}

FText SPanFightersMasterWidget::GetPlayerNameText() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloBlanco:
			return Tr(TEXT("POLLO BLANCO"), TEXT("WHITE ROOSTER"));
		case EPanFightersCharacter::PolloChocolate:
			return Tr(TEXT("POLLO CHOCOLATE"), TEXT("CHOCOLATE ROOSTER"));
		case EPanFightersCharacter::PolloAmarillo:
			return Tr(TEXT("POLLO AMARILLO"), TEXT("YELLOW ROOSTER"));
		case EPanFightersCharacter::GalloGato:
			return Tr(TEXT("GALLO GATO"), TEXT("CAT ROOSTER"));
		default:
			break;
		}
	}
	return Tr(TEXT("POLLO BLANCO"), TEXT("WHITE ROOSTER"));
}

const FSlateBrush* SPanFightersMasterWidget::GetPlayerAvatarBrush() const
{
	if (OwnerPC.IsValid())
	{
		switch (OwnerPC->GetSelectedCharacter())
		{
		case EPanFightersCharacter::PolloBlanco:
			return BrushPolloBlanco.IsValid() ? BrushPolloBlanco.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		case EPanFightersCharacter::PolloChocolate:
			return BrushPolloChocolate.IsValid() ? BrushPolloChocolate.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		case EPanFightersCharacter::PolloAmarillo:
			return BrushPolloAmarillo.IsValid() ? BrushPolloAmarillo.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		case EPanFightersCharacter::GalloGato:
			return BrushGalloGato.IsValid() ? BrushGalloGato.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
		default:
			break;
		}
	}
	return BrushPolloBlanco.IsValid() ? BrushPolloBlanco.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
}

TOptional<float> SPanFightersMasterWidget::GetRivalHealthPercent() const
{
	if (OwnerPC.IsValid())
	{
		return FMath::Clamp(OwnerPC->GetRivalCurrentHealth() / 100.0f, 0.0f, 1.0f);
	}
	return 1.0f;
}

TOptional<float> SPanFightersMasterWidget::GetRivalGhostHealthPercent() const
{
	return FMath::Clamp(RivalGhostHealth / 100.0f, 0.0f, 1.0f);
}

FSlateColor SPanFightersMasterWidget::GetRivalHealthBarColor() const
{
	const float Ratio = GetRivalHealthPercent().Get(1.0f);
	return ComputeHealthColor(Ratio);
}

FText SPanFightersMasterWidget::GetRivalHealthText() const
{
	const int32 HP = FMath::Clamp(FMath::RoundToInt(OwnerPC.IsValid() ? OwnerPC->GetRivalCurrentHealth() : 100.0f), 0, 100);
	return FText::Format(FText::FromString(TEXT("{0} / 100 HP")), FText::AsNumber(HP));
}

FText SPanFightersMasterWidget::GetRivalNameText() const
{
	if (OwnerPC.IsValid())
	{
		const FText RawName = OwnerPC->GetRivalCharacterName();
		if (bIsEnglish)
		{
			const FString NameStr = RawName.ToString();
			if (NameStr.Contains(TEXT("CHOCOLATE")))
			{
				return FText::FromString(TEXT("CHOCOLATE ROOSTER"));
			}
			if (NameStr.Contains(TEXT("BLANCO")))
			{
				return FText::FromString(TEXT("WHITE ROOSTER"));
			}
			if (NameStr.Contains(TEXT("RIVAL")))
			{
				return FText::FromString(TEXT("RIVAL ROOSTER"));
			}
		}
		return RawName;
	}
	return Tr(TEXT("GALLO RIVAL"), TEXT("RIVAL ROOSTER"));
}

const FSlateBrush* SPanFightersMasterWidget::GetRivalAvatarBrush() const
{
	return BrushPolloChocolate.IsValid() ? BrushPolloChocolate.Get() : FCoreStyle::Get().GetBrush("WhiteBrush");
}

FText SPanFightersMasterWidget::GetRoundText() const
{
	const int32 RoundNum = OwnerPC.IsValid() ? OwnerPC->GetCurrentRound() : 1;
	const FText Prefix = Tr(TEXT("ASALTO"), TEXT("ROUND"));
	return FText::FromString(FString::Printf(TEXT("%s %d/3"), *Prefix.ToString(), RoundNum));
}

FText SPanFightersMasterWidget::GetRoundScoreText() const
{
	const int32 PlayerWins = OwnerPC.IsValid() ? OwnerPC->GetPlayerRoundWins() : 0;
	const int32 RivalWins = OwnerPC.IsValid() ? OwnerPC->GetRivalRoundWins() : 0;
	return FText::FromString(FString::Printf(TEXT("%d  -  %d"), PlayerWins, RivalWins));
}

FSlateColor SPanFightersMasterWidget::ComputeHealthColor(float Ratio) const
{
	if (Ratio > 0.50f)
	{
		return FSlateColor(FLinearColor(0.18f, 0.82f, 0.28f, 1.0f));
	}
	else if (Ratio > 0.25f)
	{
		return FSlateColor(FLinearColor(0.95f, 0.65f, 0.15f, 1.0f));
	}
	else
	{
		return FSlateColor(FLinearColor(0.92f, 0.18f, 0.15f, 1.0f));
	}
}

FSlateColor SPanFightersMasterWidget::GetHitReactionColor() const
{
	return FSlateColor(FLinearColor(0.85f, 0.04f, 0.04f, HitReactionAlpha));
}

void SPanFightersMasterWidget::TriggerHitReaction(float DamageAmount)
{
	HitReactionAlpha = FMath::Clamp(0.55f + (DamageAmount / 100.0f) * 0.40f, 0.45f, 0.95f);
	PlayerHealthBarShakeTime = 0.25f;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PanFightersMenuTypes.generated.h"

UENUM(BlueprintType)
enum class EPanFightersScreenState : uint8
{
	None,
	Splash,
	MainMenu,
	OptionsMenu,
	CharacterSelect,
	StageSelect,
	InGame,
	InGameMenu,
	MatchResult
};

UENUM(BlueprintType)
enum class EPanFightersCharacter : uint8
{
	None,
	PolloBlanco     UMETA(DisplayName = "Pollo Blanco"),
	PolloChocolate  UMETA(DisplayName = "Pollo Chocolate"),
	PolloAmarillo   UMETA(DisplayName = "Pollo Amarillo"),
	GalloGato       UMETA(DisplayName = "Gallo Gato")
};

UENUM(BlueprintType)
enum class EPanFightersStage : uint8
{
	None,
	Stage1_ElRanchito   UMETA(DisplayName = "El Ranchito"),
	Stage2_ElCampoLlano UMETA(DisplayName = "El Campo Llano"),
	Stage3_ElSarten     UMETA(DisplayName = "El Sartén")
};

USTRUCT(BlueprintType)
struct FPanFightersStageInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	EPanFightersStage StageId = EPanFightersStage::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FText StageNumberText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FText StageName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FText StageSubtitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FText StageDescription;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FLinearColor ThemeColor = FLinearColor(1.0f, 0.75f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FString CameraSearchKeyword;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
	FString PlayerStartSearchKeyword;
};


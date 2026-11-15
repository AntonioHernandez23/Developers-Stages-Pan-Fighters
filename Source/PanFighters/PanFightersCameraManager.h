// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "PanFightersCameraManager.generated.h"

/**
 *  First Person camera manager for Pan Fighters.
 *  Limits look pitch and drives zero-asset procedural hitstop and impact camera shake.
 */
UCLASS()
class APanFightersCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	APanFightersCameraManager();

	/** Starts a damped procedural camera shake on melee impact. */
	UFUNCTION(BlueprintCallable, Category="Combat|Feedback")
	void TriggerImpactShake(float Strength, float DurationSeconds);

	/** Briefly slows global time dilation for hitstop using real unscaled wall-clock time. */
	UFUNCTION(BlueprintCallable, Category="Combat|Feedback")
	void TriggerHitstop(float RealDurationSeconds, float TimeDilation = 0.05f);

protected:

	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	void ClearHitstopIfElapsed();
	void RestoreTimeDilation();

	bool bHitstopActive = false;
	double HitstopEndRealTime = 0.0;

	float ShakeTimeRemaining = 0.0f;
	float ShakeTotalDuration = 0.0f;
	float ShakeIntensity = 0.0f;
	float ShakeElapsed = 0.0f;
	int32 ShakeSeed = 0;
};

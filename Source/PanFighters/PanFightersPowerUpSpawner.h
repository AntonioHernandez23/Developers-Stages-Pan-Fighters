#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PanFightersPowerUpSpawner.generated.h"

UCLASS()
class PANFIGHTERS_API APanFightersPowerUpSpawner : public AActor
{
	GENERATED_BODY()

public:
	APanFightersPowerUpSpawner();

protected:
	virtual void BeginPlay() override;

public:
	/** Clases de Power-Ups disponibles para spawnear al azar */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PowerUp Spawner")
	TArray<TSubclassOf<AActor>> PowerUpClasses;

	/** Intervalo en segundos entre cada spawn */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PowerUp Spawner", meta = (ClampMin = "1.0"))
	float SpawnInterval = 10.0f;

	/** Radio en cm alrededor del spawner donde puede aparecer el item */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PowerUp Spawner", meta = (ClampMin = "50.0"))
	float SpawnRadius = 450.0f;

	/** Altura adicional en Z sobre el spawner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PowerUp Spawner")
	float SpawnHeightOffset = 60.0f;

	/** Si true, spawnea un item inmediatamente al iniciar la partida */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PowerUp Spawner")
	bool bSpawnOnBeginPlay = true;

	/** Máximo de items activos simultáneamente generados por este spawner */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PowerUp Spawner", meta = (ClampMin = "1"))
	int32 MaxActivePowerUps = 3;

	UFUNCTION(BlueprintCallable, Category = "PowerUp Spawner")
	void SpawnRandomPowerUp();

private:
	FTimerHandle SpawnTimerHandle;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> ActiveSpawnedItems;

	void CleanupDestroyedItems();
};

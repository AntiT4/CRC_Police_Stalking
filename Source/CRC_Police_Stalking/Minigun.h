// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Minigun.generated.h"

UCLASS()
class CRC_POLICE_STALKING_API AMinigun : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AMinigun();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	//============== 추가 코드 ================
public:
	// 외관을 만들기 위한 컴포넌트
	UPROPERTY(VisibleAnywhere)
		USceneComponent* mpRoot = nullptr;

	UPROPERTY(VisibleAnywhere)
		UStaticMeshComponent* mpBody = nullptr;
};

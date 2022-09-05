// Fill out your copyright notice in the Description page of Project Settings.


#include "GolfClub.h"

// Sets default values
AGolfClub::AGolfClub()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	mpRoot = CreateDefaultSubobject<USceneComponent>(TEXT("instSceneRoot"));
	SetRootComponent(mpRoot);

	mpBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("instBody"));
	mpBody->SetupAttachment(mpRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> tPFMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	if (tPFMesh.Succeeded())
	{
		mpBody->SetStaticMesh(tPFMesh.Object);
		mpBody->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
		mpBody->SetRelativeScale3D(FVector(0.4f, 0.4f, 0.4f));
	}
}

// Called when the game starts or when spawned
void AGolfClub::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AGolfClub::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


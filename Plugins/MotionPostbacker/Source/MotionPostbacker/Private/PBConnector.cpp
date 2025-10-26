// Fill out your copyright notice in the Description page of Project Settings.


#include "PBConnector.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CommandResolver.h"

// Sets default values
APBConnector::APBConnector()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	td = nullptr;
	threadConnect = nullptr;
}

// Called when the game starts or when spawned
void APBConnector::BeginPlay()
{
	Super::BeginPlay();
}

void APBConnector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
}

// Called every frame
void APBConnector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FString payload;
	while (SendQueue.Dequeue(payload))
		SendString(payload);
}

void APBConnector::TryConnectServer(const FString& address, int32 port, bool useUdp, bool logMessage)
{
	bUseUdp = useUdp;
	bLogMessage = logMessage;

	UE_LOG(LogTemp, Log, TEXT("TryConnectServer"));

	td = new ThreadDispatcher(address, port, useUdp, logMessage);
	onUpdate = FTimerDelegate::CreateUObject(this, &APBConnector::ThreadCreate);
	GetWorld()->GetTimerManager().SetTimer(countdownTimerHandle, onUpdate, 0.001f, false);
}

bool APBConnector::IsConnected() 
{
	return td && td->IsConnected();
}

void APBConnector::Stop()
{
	if (threadConnect)
	{
		threadConnect->Kill(true);
		delete threadConnect;
		threadConnect = nullptr;
	}

	if (td)
	{
		td->Stop();
		delete td;
		td = nullptr;
	}
}

bool APBConnector::SendString(const FString& Message)
{
	if (!td)
	{
		UE_LOG(LogTemp, Warning, TEXT("Send failed: ThreadDispatcher not initialized"));
		return false;
	}

	bool ok = td->SendString(Message);
	if (bLogMessage)
	{
		UE_LOG(LogTemp, Log, TEXT("Sent: %s"), *Message);
	}
	return ok;
}

void APBConnector::EnqueueJson(const FString& JsonString)
{
	SendQueue.Enqueue(JsonString);
}

void APBConnector::ThreadCreate()
{
	if (threadConnect)
		return;

	threadConnect = FRunnableThread::Create(td, TEXT("Socket Thread"));
}

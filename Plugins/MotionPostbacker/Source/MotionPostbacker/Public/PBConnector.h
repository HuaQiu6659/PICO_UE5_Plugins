// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ThreadDispatcher.h"
#include "..\Source\Runtime\Engine\Public\TimerManager.h"
#include "Sockets.h"
#include "Networking.h"
#include "Containers/Queue.h"
#include "PBConnector.generated.h"

UCLASS()
class MOTIONPOSTBACKER_API APBConnector : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	APBConnector();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "Socket")
		void TryConnectServer(const FString& address = TEXT("127.0.0.1"), int32 port = 6666, bool useUdp = false, bool logMessage = false);
	
	UFUNCTION(BlueprintCallable, Category = "Socket")
		bool IsConnected();

	UFUNCTION(BlueprintCallable, Category = "Socket")
		void Stop();


	// 1) FString 发送接口
	UFUNCTION(BlueprintCallable, Category = "Socket")
		bool SendString(const FString& Message);

	// 3) 数据入队接口
	UFUNCTION(BlueprintCallable, Category = "Socket")
		void EnqueueJson(const FString& JsonString);

private:
	/*---------------------Socket---------------------*/
	FRunnableThread* threadConnect;
	ThreadDispatcher* td;

	// 发送队列与配置
	bool bUseUdp = false;
	bool bLogMessage = false;
	TQueue<FString> SendQueue;

	FTimerHandle					countdownTimerHandle;
	FTimerDelegate					onUpdate;

	void							ThreadCreate();
};

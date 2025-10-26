// Fill out your copyright notice in the Description page of Project Settings.


#include "PBConnector.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "CommandResolver.h"
#include "CommandBuilder.h"

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
	while (sendQueue.Dequeue(payload))
		SendString(payload);

	if (td && td->IsConnected())
	{
		// 仅在定时器仍激活时清理，避免每帧无意义 Clear
		if (GetWorld()->GetTimerManager().IsTimerActive(connectTimeoutTimerHandle))
			GetWorld()->GetTimerManager().ClearTimer(connectTimeoutTimerHandle);

		// 仅在状态首次变为已连接时广播
		if (!bConnectedNotified)
		{
			onConnectorStateChanged.Broadcast(EConnectorState::Connected);
			bConnectedNotified = true;
		}
	}
	else
	{
		// 未连接时重置标记，允许下一次连接成功时广播一次
		bConnectedNotified = false;
	}
}

void APBConnector::TryConnectServer(const FString& address, int32 port, bool useUdp, bool logMessage)
{
	// 校验地址与端口，非法则直接返回，不创建线程
	bool bValidIp = false;
	TSharedRef<FInternetAddr> testAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	testAddr->SetIp(*address, bValidIp);
	const bool bValidPort = port > 0 && port <= 65535;

	if (!bValidIp || !bValidPort)
	{
		FString reason;
		if (!bValidIp) 
			reason += TEXT("地址无效");

		if (!bValidPort) 
			reason += reason.IsEmpty() ? TEXT("端口无效") : TEXT("，端口无效");

		FString warn = FString::Printf(TEXT("连接失败: %s:%d，原因：%s"), *address, port, *reason);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
		UCommandResolver::GetInstance()->onMotionUpdate.Broadcast(warn);
		return;
	}

	Stop();
	bUseUdp = useUdp;
	bLogMessage = logMessage;

	// 每次开始连接时重置通知标记
	bConnectedNotified = false;

	// 记录目标地址与端口供超时回调使用
	connectAddress = address;
	connectPort = port;

	onConnectorStateChanged.Broadcast(EConnectorState::Connecting);

	FString info = FString::Printf(TEXT("尝试连接服务器 %s:%d"), *address, port);
	UCommandResolver::GetInstance()->onMotionUpdate.Broadcast(info);

	td = new ThreadDispatcher(address, port, useUdp, logMessage);
	onUpdate = FTimerDelegate::CreateUObject(this, &APBConnector::ThreadCreate);
	GetWorld()->GetTimerManager().SetTimer(countdownTimerHandle, onUpdate, 0.001f, false);

	// 设置5秒连接超时定时器
	GetWorld()->GetTimerManager().SetTimer(connectTimeoutTimerHandle, this, &APBConnector::OnConnectTimeout, 5.0f, false);
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

	// 停止时清理连接超时定时器
	GetWorld()->GetTimerManager().ClearTimer(connectTimeoutTimerHandle);

	// 停止时也重置通知标记
	bConnectedNotified = false;

	onConnectorStateChanged.Broadcast(EConnectorState::Unconnect);
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
	sendQueue.Enqueue(JsonString);
}

void APBConnector::SendGlobalConfigCommand(const FString& clipperSn, const FString& dummySn)
{
	auto jsonStr = CommandBuilder::GlobalConfigCommand(clipperSn, dummySn);
	EnqueueJson(jsonStr);
}

void APBConnector::SendTrackerDatas(const TArray<FTrackerData> datas)
{
	auto jsonStr = CommandBuilder::TrackerDatas(datas);
	EnqueueJson(jsonStr);
}

void APBConnector::ThreadCreate()
{
	if (threadConnect)
		return;

	threadConnect = FRunnableThread::Create(td, TEXT("Socket Thread"));
}

void APBConnector::OnConnectTimeout()
{
	// 如果仍未连接成功，广播失败并停止
	if (!td || !td->IsConnected())
	{
		FString msg = FString::Printf(TEXT("连接超时: %s:%d"), *connectAddress, connectPort);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *msg);
		UCommandResolver::GetInstance()->onMotionUpdate.Broadcast(msg);
		Stop();
	}
}
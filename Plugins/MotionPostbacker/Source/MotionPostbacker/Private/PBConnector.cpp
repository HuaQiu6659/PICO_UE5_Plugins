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
		if (GetWorld()->GetTimerManager().IsTimerActive(connectTimeoutTimerHandle))
			GetWorld()->GetTimerManager().ClearTimer(connectTimeoutTimerHandle);

		if (!connectedNotified)
		{
			onConnectorStateChanged.Broadcast(EConnectorState::Connected);
			connectedNotified = true;
		}
	}
	else
	{
		// 如果之前已经通知为连接成功，而当前检测到未连接，则广播断开状态
		if (connectedNotified)
		{
			onConnectorStateChanged.Broadcast(EConnectorState::Unconnect);
			connectedNotified = false;
		}
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
		UCommandResolver::GetInstance()->onMessageUpdate.Broadcast(warn, EMessageType::Message);
		return;
	}

	Stop();

	bUseUdp = useUdp;
	bLogMessage = logMessage;
	connectedNotified = false;
	connectAddress = address;
	connectPort = port;

	onConnectorStateChanged.Broadcast(EConnectorState::Connecting);

	FString info = FString::Printf(TEXT("尝试连接服务器 %s:%d"), *address, port);
	UCommandResolver::GetInstance()->onMessageUpdate.Broadcast(info, EMessageType::Message);

	td = new ThreadDispatcher(address, port, useUdp, logMessage);
	onUpdate = FTimerDelegate::CreateUObject(this, &APBConnector::ThreadCreate);
	GetWorld()->GetTimerManager().SetTimer(countdownTimerHandle, onUpdate, 0.001f, false);

	// 设置10秒连接超时定时器
	GetWorld()->GetTimerManager().SetTimer(connectTimeoutTimerHandle, this, &APBConnector::OnConnectTimeout, 10, false);
}

bool APBConnector::IsConnected() 
{
	return td && td->IsConnected();
}

void APBConnector::Stop()
{
    if (td)
        td->Stop();

    if (threadConnect)
    {
        threadConnect->WaitForCompletion();
        delete threadConnect;
        threadConnect = nullptr;
    }

    if (td)
    {
        delete td;
        td = nullptr;
    }

    if (GetWorld()->GetTimerManager().IsTimerActive(countdownTimerHandle))
        GetWorld()->GetTimerManager().ClearTimer(countdownTimerHandle);

    GetWorld()->GetTimerManager().ClearTimer(connectTimeoutTimerHandle);
    connectedNotified = false;
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

void APBConnector::EnqueueJson(const FString& jsonString)
{
	if (jsonString.IsEmpty())
		return;

    FString payload = jsonString;
	payload.ReplaceInline(TEXT("\r"), TEXT(""));
	payload.ReplaceInline(TEXT("\n"), TEXT(""));
    if (!payload.EndsWith(TEXT("\r\n")))
    {
        while (payload.Len() > 0 && (payload.EndsWith(TEXT("\r")) || payload.EndsWith(TEXT("\n"))))
            payload.RemoveAt(payload.Len() - 1, 1, false);
        payload.Append(TEXT("\r\n"));
    }
    sendQueue.Enqueue(payload);
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

void APBConnector::SendStartCommand(EMotionType motionType)
{
	auto jsonStr = CommandBuilder::StartCommand(motionType);
	EnqueueJson(jsonStr);
}

void APBConnector::SendEndCommand(EMotionType motionType)
{
	auto jsonStr = CommandBuilder::EndCommand(motionType);
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
	if (!td || !td->IsConnected())
	{
		FString msg = FString::Printf(TEXT("连接超时: %s:%d"), *connectAddress, connectPort);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *msg);
		UCommandResolver::GetInstance()->onMessageUpdate.Broadcast(msg, EMessageType::Message);
		Stop();
	}
}
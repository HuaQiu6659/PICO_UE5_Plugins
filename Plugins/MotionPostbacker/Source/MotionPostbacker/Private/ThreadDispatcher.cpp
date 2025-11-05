// Fill out your copyright notice in the Description page of Project Settings.


#include "ThreadDispatcher.h"
#include "..\Source\Runtime\Core\Public\Containers\StringConv.h"
#include "..\Source\Runtime\Json\Public\Serialization\JsonReader.h"
#include "..\Source\Runtime\Core\Public\Templates\SharedPointer.h"
#include "..\Source\Runtime\Json\Public\Serialization\JsonSerializer.h"
#include "..\Source\Runtime\Json\Public\Dom\JsonObject.h"
#include "Async/Async.h"
#include "CommandResolver.h"
#include "Engine/Engine.h"

ThreadDispatcher::ThreadDispatcher(const FString& address, int32 port, bool useUdp, bool logMessage)
{
	this->address = address;
	this->port = port;
	this->udp = useUdp;
	this->logMessage = logMessage;
}

ThreadDispatcher::~ThreadDispatcher()
{
}

bool ThreadDispatcher::Init()
{
    shouldStop = false;
    connected = false;
    return true;
}

uint32 ThreadDispatcher::Run()
{
	FPlatformProcess::Sleep(0.03f);	
	UE_LOG(LogTemp, Log, TEXT("Thread start run."));

	if (udp)
		UdpRecv();
	else
		TcpRecv();

	return 1;
}

void ThreadDispatcher::Stop()
{
    // 仅标记停止并更新连接状态，不在此处销毁 socket，避免与接收循环并发冲突
    shouldStop = true;
    connected = false;
}

void ThreadDispatcher::TcpRecv()
{
	// 创建套接字
	socket = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateSocket(NAME_Stream, TEXT("TcpClient"));

	if (!socket)
	{
		UE_LOG(LogTemp, Error, TEXT("Socket is null."));
		return;
	}
    // 采用非阻塞模式，配合主动轮询连接状态，避免失败时长时间阻塞
    socket->SetNonBlocking(true);

	// 连接到服务器
	TSharedRef<FInternetAddr> remoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
	bool isValid;
	remoteAddress->SetIp(*address, isValid);
	remoteAddress->SetPort(port);

	if (!isValid)
	{
		UE_LOG(LogTemp, Error, TEXT("Address is not vaild."));
		return;
	}

    // 发起连接（非阻塞），然后统一通过状态轮询判定是否真正连接成功
    socket->Connect(*remoteAddress);
    const double startSec = FPlatformTime::Seconds();
    const double timeoutSec = 1.0; // 连接超时时间，缩短失败卡顿
    while (!shouldStop)
    {
        ESocketConnectionState state = socket->GetConnectionState();
        if (state == ESocketConnectionState::SCS_Connected)
        {
            connected = true;
            break;
        }
        if (state == ESocketConnectionState::SCS_ConnectionError)
        {
            UE_LOG(LogTemp, Error, TEXT("Tcp connect error to %s:%d"), *address, port);
            FString msg = FString::Printf(TEXT("Tcp connect error to %s:%d"), *address, port);
            AsyncTask(ENamedThreads::GameThread, [text = msg]() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.016f, FColor::Red, text); });
            // 连接失败，关闭套接字并返回
            FScopeLock lock(&socketMutex);
            if (socket)
            {
                socket->Close();
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(socket);
                socket = nullptr;
            }
            return;
        }

        // 超时
        if (FPlatformTime::Seconds() - startSec > timeoutSec)
        {
            UE_LOG(LogTemp, Error, TEXT("Tcp connect timeout %s:%d"), *address, port);
            FString msg = FString::Printf(TEXT("Tcp connect timeout %s:%d"), *address, port);
            AsyncTask(ENamedThreads::GameThread, [text = msg]() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.016f, FColor::Yellow, text); });
            // 连接超时，关闭套接字并返回
            FScopeLock lock(&socketMutex);
            if (socket)
            {
                socket->Close();
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(socket);
                socket = nullptr;
            }
            return;
        }

        FPlatformProcess::Sleep(0.01f);
    }
    if (shouldStop)
    {
        // 外部请求停止，清理并退出
        FScopeLock lock(&socketMutex);
        if (socket)
        {
            socket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(socket);
            socket = nullptr;
        }
        return;
    }

	remoteAddr = remoteAddress;

	// 连接成功后开始接收数据
	FPlatformProcess::Sleep(0.1f);
	uint32 size;
    while (!shouldStop)
    {
        // 实时检测连接状态，服务端关闭时及时退出循环并更新状态
        const ESocketConnectionState connState = socket->GetConnectionState();
        if (connState != ESocketConnectionState::SCS_Connected)
        {
            connected = false;
            UE_LOG(LogTemp, Warning, TEXT("Tcp disconnected: %s:%d"), *address, port);
            FString msg = FString::Printf(TEXT("Tcp disconnected: %s:%d"), *address, port);
            AsyncTask(ENamedThreads::GameThread, [text = msg]() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, text); });
            break;
        }

        if (!socket->HasPendingData(size))
        {
            // 无数据时短暂休眠，避免忙等导致高 CPU 或在安卓上触发异常
            FPlatformProcess::Sleep(0.005f);
            continue;
        }

		receiveData.SetNumUninitialized(FMath::Min(size, 65507u));
		int32 bytesRead = 0;
		if (socket->Recv(receiveData.GetData(), receiveData.Num(), bytesRead))
			NewData(bytesRead);
    }
    UE_LOG(LogTemp, Log, TEXT("Tcp stop."));
    connected = false;
    // 在接收循环结束后统一关闭并销毁 socket，保证线程安全
    {
        FScopeLock lock(&socketMutex);
        if (socket)
        {
            socket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(socket);
            socket = nullptr;
        }
    }
}

void ThreadDispatcher::UdpRecv()
{
	socket = FUdpSocketBuilder(TEXT("UdpReceiver"))
		.BoundToAddress(FIPv4Address::Any)
		.BoundToPort(port)
		.WithBroadcast()
		.Build();

	if (!socket)
	{
		UE_LOG(LogTemp, Error, TEXT("Socket is null."));
		return;
	}
	FPlatformProcess::Sleep(0.1f);

    uint32 size;
    while (!shouldStop)
    {
        if (!socket->HasPendingData(size))
        {
            // 无数据时短暂休眠，避免忙等导致高 CPU 或在安卓上触发异常
            FPlatformProcess::Sleep(0.005f);
            continue;
        }

		receiveData.SetNumUninitialized(FMath::Min(size, 65507u));
		int32 bytesRead = 0;
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		if (socket->RecvFrom(receiveData.GetData(), receiveData.Num(), bytesRead, *Sender))
			NewData(bytesRead);
    }
    UE_LOG(LogTemp, Log, TEXT("Udp stop."));
    connected = false;
    // 在接收循环结束后统一关闭并销毁 socket，保证线程安全
    {
        FScopeLock lock(&socketMutex);
        if (socket)
        {
            socket->Close();
            ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(socket);
            socket = nullptr;
        }
    }
}

void ThreadDispatcher::NewData(int32 bytesRead)
{
	FUTF8ToTCHAR converter((const ANSICHAR*)receiveData.GetData(), bytesRead);
	FString chunk(converter.Length(), converter.Get());
	receiveData.Empty();
	receiveBuffer.Append(chunk);

	int32 lastNewlineIdx = receiveBuffer.Find(TEXT("\n"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	if (lastNewlineIdx == INDEX_NONE)
		return;

	FString processArea = receiveBuffer.Left(lastNewlineIdx + 1);
	receiveBuffer = receiveBuffer.Mid(lastNewlineIdx + 1);
    AsyncTask(ENamedThreads::GameThread, [text = processArea]() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 1, FColor::White, text); });

	TArray<FString> lines;
	processArea.ParseIntoArray(lines, TEXT("\n"), false);
	for (FString& line : lines)
	{
		while (line.Len() > 0 && line.EndsWith(TEXT("\r")))
			line.RemoveAt(line.Len() - 1, 1, false);

		line.TrimStartAndEndInline();
		if (line.IsEmpty())
			continue;

		if (logMessage)
			UE_LOG(LogTemp, Log, TEXT("Socket Recv Line: %s"), *line);

		TSharedPtr<FJsonObject> jsonObject;
		TSharedRef<TJsonReader<TCHAR>> jsonReader = TJsonReaderFactory<TCHAR>::Create(line);
		bool isVaild = FJsonSerializer::Deserialize(jsonReader, jsonObject);
		if (!isVaild || !jsonObject.IsValid())
			continue;

		AsyncTask(ENamedThreads::GameThread,
			[json = MoveTemp(line)]()
			{
				UCommandResolver::GetInstance()->Resolve(json);
			});
	}
}

bool ThreadDispatcher::SendString(const FString& message)
{
    if (message.IsEmpty())
		return false;

    FScopeLock lock(&socketMutex);
    if (!socket)
    {
        UE_LOG(LogTemp, Warning, TEXT("Send failed: socket is null."));
        return false;
	}

	FTCHARToUTF8 converter(*message);
	int32 bytesToSend = converter.Length();
	int32 sent = 0;

	if (udp)
	{
		if (!remoteAddr.IsValid())
		{
			TSharedRef<FInternetAddr> remoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
			bool bValid = false;
			remoteAddress->SetIp(*address, bValid);
			remoteAddress->SetPort(port);

        if (!bValid)
        {
            UE_LOG(LogTemp, Error, TEXT("Send failed: invalid address %s"), *address);
            FString msg = FString::Printf(TEXT("Send failed: invalid address %s"), *address);;
            AsyncTask(ENamedThreads::GameThread, [text = msg]() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.016f, FColor::Yellow, text); });
            return false;
        }
        remoteAddr = remoteAddress;
    }

		bool bOk = socket->SendTo((uint8*)converter.Get(), bytesToSend, sent, *remoteAddr);
        if (!bOk || sent != bytesToSend)
        {
            UE_LOG(LogTemp, Warning, TEXT("UDP send partial or failed. Sent=%d, Total=%d"), sent, bytesToSend);
            FString msg = FString::Printf(TEXT("UDP send partial or failed. Sent=%d, Total=%d"), sent, bytesToSend);
            AsyncTask(ENamedThreads::GameThread, [text = msg]() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.016f, FColor::Yellow, text); });
            return false;
        }
        return true;
    }
    else
	{
		bool bOk = socket->Send((uint8*)converter.Get(), bytesToSend, sent);
        if (!bOk || sent != bytesToSend)
        {
            UE_LOG(LogTemp, Warning, TEXT("TCP send partial or failed. Sent=%d, Total=%d"), sent, bytesToSend);
            FString msg = FString::Printf(TEXT("TCP send partial or failed. Sent=%d, Total=%d"), sent, bytesToSend);
            AsyncTask(ENamedThreads::GameThread, [text = msg]() { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 0.016f, FColor::Yellow, text); });
            return false;
        }
        return true;
    }
}
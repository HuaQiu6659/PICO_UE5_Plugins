// Fill out your copyright notice in the Description page of Project Settings.


#include "ThreadDispatcher.h"
#include "..\Source\Runtime\Core\Public\Containers\StringConv.h"
#include "..\Source\Runtime\Json\Public\Serialization\JsonReader.h"
#include "..\Source\Runtime\Core\Public\Templates\SharedPointer.h"
#include "..\Source\Runtime\Json\Public\Serialization\JsonSerializer.h"
#include "..\Source\Runtime\Json\Public\Dom\JsonObject.h"
#include "Async/Async.h"
#include "CommandResolver.h"

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
	shouldStop = true;
	connected = false;

	FScopeLock lock(&socketMutex);
	if (socket)
	{
		socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(socket);
		socket = nullptr;
	}
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
	socket->SetNonBlocking(false);

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

	if (!socket->Connect(*remoteAddress))
	{
		UE_LOG(LogTemp, Error, TEXT("Tcp fail to connect %s:%d."), *address, port);
		FString msg = FString::Printf(TEXT("Tcp fail to connect %s:%d."), *address, port);
		return;
	}

	remoteAddr = remoteAddress;

	// 连接成功后开始接收数据
	FPlatformProcess::Sleep(0.1f);
	uint32 size;
	connected = true;
	while (!shouldStop)
	{
		if (!socket->HasPendingData(size))
			continue;

		receiveData.SetNumUninitialized(FMath::Min(size, 65507u));
		int32 bytesRead = 0;
		if (socket->Recv(receiveData.GetData(), receiveData.Num(), bytesRead))
			NewData(bytesRead);
	}
	UE_LOG(LogTemp, Log, TEXT("Tcp stop."));
    connected = false;
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
			continue;

		receiveData.SetNumUninitialized(FMath::Min(size, 65507u));
		int32 bytesRead = 0;
		TSharedRef<FInternetAddr> Sender = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		if (socket->RecvFrom(receiveData.GetData(), receiveData.Num(), bytesRead, *Sender))
			NewData(bytesRead);
	}
	UE_LOG(LogTemp, Log, TEXT("Udp stop."));
    connected = false;
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
				return false;
			}
			remoteAddr = remoteAddress;
		}

		bool bOk = socket->SendTo((uint8*)converter.Get(), bytesToSend, sent, *remoteAddr);
		if (!bOk || sent != bytesToSend)
		{
			UE_LOG(LogTemp, Warning, TEXT("UDP send partial or failed. Sent=%d, Total=%d"), sent, bytesToSend);
			FString msg = FString::Printf(TEXT("UDP send partial or failed. Sent=%d, Total=%d"), sent, bytesToSend);
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
			return false;
		}
		return true;
	}
}
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
	UE_LOG(LogTemp, Log, TEXT("Thead start run."));

	if (udp)
		UdpRecv();
	else
		TcpRecv();

	return 1;
}

void ThreadDispatcher::Stop()
{
	shouldStop = true;
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
		return;
	}

	// 记录 RemoteAddr 供发送使用
	RemoteAddr = remoteAddress;

	FPlatformProcess::Sleep(0.1f);

	// 连接成功后开始接收数据
	uint32 size;
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
}

void ThreadDispatcher::NewData(int32 BytesRead)
{
	//FString jsonStr = FString(reinterpret_cast<const char*>(receiveData.GetData())).Left(BytesRead);
	FUTF8ToTCHAR converter((const ANSICHAR*)receiveData.GetData(), BytesRead);
	FString jsonStr = FString(converter.Length(), converter.Get());
	connected = true;
	receiveData.Empty();

	if (logMessage)
		UE_LOG(LogTemp, Log, TEXT("Socket Recv: %s"), *jsonStr);
	
	TSharedPtr<FJsonObject> jsonObject;
	TSharedRef<TJsonReader<TCHAR>> jsonReader = TJsonReaderFactory<TCHAR>::Create(jsonStr);
	bool bFlag = FJsonSerializer::Deserialize(jsonReader, jsonObject);	//是否能反序列化该字符串是否是合法的
	if (!bFlag || !jsonObject.IsValid())
		return;


	FString jsonCopy = jsonStr;
	AsyncTask(ENamedThreads::GameThread, 
		[jsonCopy]()
		{
			UCommandResolver::GetInstance()->Resolve(jsonCopy);
		});
}

bool ThreadDispatcher::SendString(const FString& Message)
{
	if (!socket)
	{
		UE_LOG(LogTemp, Warning, TEXT("Send failed: socket is null."));
		return false;
	}

	FTCHARToUTF8 Converter(*Message);
	int32 BytesToSend = Converter.Length();
	int32 Sent = 0;

	if (udp)
	{
		// 准备远端地址（如果还未初始化）
		if (!RemoteAddr.IsValid())
		{
			TSharedRef<FInternetAddr> remoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
			bool bValid = false;
			remoteAddress->SetIp(*address, bValid);
			remoteAddress->SetPort(port);
			if (!bValid)
			{
				UE_LOG(LogTemp, Error, TEXT("Send failed: invalid address %s"), *address);
				return false;
			}
			RemoteAddr = remoteAddress;
		}

		bool bOk = socket->SendTo((uint8*)Converter.Get(), BytesToSend, Sent, *RemoteAddr);
		if (!bOk || Sent != BytesToSend)
		{
			UE_LOG(LogTemp, Warning, TEXT("UDP send partial or failed. Sent=%d, Total=%d"), Sent, BytesToSend);
			return false;
		}
		return true;
	}
	else
	{
		bool bOk = socket->Send((uint8*)Converter.Get(), BytesToSend, Sent);
		if (!bOk || Sent != BytesToSend)
		{
			UE_LOG(LogTemp, Warning, TEXT("TCP send partial or failed. Sent=%d, Total=%d"), Sent, BytesToSend);
			return false;
		}
		return true;
	}
}
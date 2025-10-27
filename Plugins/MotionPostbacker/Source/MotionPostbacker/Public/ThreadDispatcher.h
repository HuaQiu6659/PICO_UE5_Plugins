// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../Source/Runtime/Core/Public/Containers/Queue.h"
#include "../Source/Runtime/Core/Public/HAL/Runnable.h"
#include "../Source/Runtime/Sockets/Public/Sockets.h"
#include "Networking.h"
#include "HAL/CriticalSection.h"

class MOTIONPOSTBACKER_API ThreadDispatcher : public FRunnable
{
public:
	ThreadDispatcher(const FString& address, int32 port, bool useUdp, bool logMessage);
	~ThreadDispatcher();

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;

	// 发送接口：直接使用本类的 socket
	bool SendString(const FString& Message);

	bool IsConnected() const { return connected; }

private:

	FString address;
	int port;
    bool udp = false;
    bool shouldStop = false;
    bool connected = false;
    bool logMessage = false;
    TArray<uint8> receiveData;
    FString receiveBuffer;

    FSocket* socket = nullptr;
    FCriticalSection socketMutex;
    TSharedPtr<FInternetAddr> remoteAddr;
    void NewData(int32 bytesRead);

	//TCP
	void TcpRecv();

	//UDP
	void UdpRecv();
};
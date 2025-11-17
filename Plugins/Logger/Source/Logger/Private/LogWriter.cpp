// Fill out your copyright notice in the Description page of Project Settings.


#include "LogWriter.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "UObject/Package.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

static ULogWriter* gInstance = nullptr;

ULogWriter* ULogWriter::GetLogWriter()
{
    if (gInstance) return gInstance;
    gInstance = NewObject<ULogWriter>(GetTransientPackage(), ULogWriter::StaticClass());
    gInstance->AddToRoot();
    gInstance->Initialize();
    return gInstance;
}

void ULogWriter::Log(const FString& message)
{
    EnsureFilePathInitialized(TEXT("Log"), logFilePath);
    WriteLine(message, logFilePath);
}

void ULogWriter::Warning(const FString& message)
{
    EnsureFilePathInitialized(TEXT("Warning"), warningFilePath);
    WriteLine(message, warningFilePath);
}

void ULogWriter::Error(const FString& message)
{
    EnsureFilePathInitialized(TEXT("Error"), errorFilePath);
    WriteLine(message, errorFilePath);
}

void ULogWriter::Initialize()
{
    FScopeLock lock(&writeLock);
    const FString dir = GetLogDir();
    IFileManager::Get().MakeDirectory(*dir, true);
}

FString ULogWriter::GetLogDir() const
{
    return FPaths::Combine(FPaths::ProjectLogDir(), TEXT("Logs"));
}

FString ULogWriter::GetTodayLogFilePath(const FString& category) const
{
    const FString date = FDateTime::Now().ToString(TEXT("yyyyMMdd"));
    return FPaths::Combine(GetLogDir(), FString::Printf(TEXT("%s-%s.log"), *category, *date));
}

void ULogWriter::WriteLine(const FString& content, const FString& filePath)
{
    FScopeLock lock(&writeLock);
    const FString time = FDateTime::Now().ToString(TEXT("HH:mm:ss.SSS"));
    const FString line = FString::Printf(TEXT("[%s][%s]\n"), *time, *content);
    FFileHelper::SaveStringToFile(line, *filePath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}

void ULogWriter::EnsureFilePathInitialized(const FString& category, FString& filePath)
{
    if (!filePath.IsEmpty()) return;
    FScopeLock lock(&writeLock);
    if (filePath.IsEmpty()) filePath = GetTodayLogFilePath(category);
}

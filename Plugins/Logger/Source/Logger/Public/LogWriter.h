// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "LogWriter.generated.h"

/**
 * 
 */
UCLASS()
class LOGGER_API ULogWriter : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category = "Logger")
        static ULogWriter* GetLogWriter();

    UFUNCTION(BlueprintCallable, Category = "Logger")
        void Log(const FString& message);
    UFUNCTION(BlueprintCallable, Category = "Logger")
        void Warning(const FString& message);
    UFUNCTION(BlueprintCallable, Category = "Logger")
        void Error(const FString& message);

private:
    FString logFilePath;
    FString warningFilePath;
    FString errorFilePath;
    FCriticalSection writeLock;

    void Initialize();
    void WriteLine(const FString& content, const FString& filePath);
    void EnsureFilePathInitialized(const FString& category, FString& filePath);
    FString GetLogDir() const;
    FString GetTodayLogFilePath(const FString& category) const;
};

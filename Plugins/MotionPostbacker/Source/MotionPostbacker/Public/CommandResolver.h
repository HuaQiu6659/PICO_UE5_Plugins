// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "HAL/CriticalSection.h"
#include "Runtime/Json/Public/Dom/JsonObject.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Enums.h"
#include "CommandResolver.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMessageDelegate, const FString&, message, EMessageType, messageType);

UCLASS()
class MOTIONPOSTBACKER_API UCommandResolver : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Motion")
		static UCommandResolver* GetInstance();

    UFUNCTION(BlueprintCallable, Category = "Motion")
        FString GetBizId();

	UPROPERTY(BlueprintAssignable, Category="Motion")
		FMessageDelegate onMessageUpdate;

	void Resolve(const FString& json);
	void Set(const FString& key, const FString& value);
	FString Get(const FString& key);

private:
	static const int32 SUCCESS_CODE = 1000;
	static UCommandResolver* Instance;

	TMap<FString, FString> idMap;
	FCriticalSection idMapMutex;

	void Initialize();

	void OnRescueAppConfig(const TSharedPtr<FJsonObject>& json);

	// trajectory
	void OnTrajectoryAnalysis(const TSharedPtr<FJsonObject>& json);
	void OnTrajectoryAnalysis_Begin(const TSharedPtr<FJsonObject>& json);
	void OnTrajectoryAnalysis_Stop(const TSharedPtr<FJsonObject>& json);
	void OnTrajectoryAnalysis_TrReport(const TSharedPtr<FJsonObject>& json);
	void OnTrajectoryAnalysis_Result(const TSharedPtr<FJsonObject>& json);

	// cpr
	void OnCprAnalysis(const TSharedPtr<FJsonObject>& json);
	void OnCprAnalysis_Begin(const TSharedPtr<FJsonObject>& json);
	void OnCprAnalysis_End(const TSharedPtr<FJsonObject>& json);
	void OnCprAnalysis_Result(const TSharedPtr<FJsonObject>& json);
};

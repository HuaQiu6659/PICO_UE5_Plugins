// Fill out your copyright notice in the Description page of Project Settings.


#include "CommandBuilder.h"
#include "CommandResolver.h"
#include "Json.h"
#include "JsonUtilities.h"

const FString& CommandBuilder::GlobalConfigCommand(const FString& clipperSn, const FString& dummySn)
{
	static FString cachedJson;

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());
	root->SetStringField(TEXT("cmd"), TEXT("rescueAppConfig"));
	root->SetNumberField(TEXT("fps"), 60);
	root->SetNumberField(TEXT("engine"), 1);
	root->SetStringField(TEXT("asepticClipper"), clipperSn);
	root->SetStringField(TEXT("dummy"), dummySn);

	cachedJson.Empty();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), Writer);
	return cachedJson;
}

const FString& CommandBuilder::StartCommand(EMotionType motionType)
{
	static FString cachedJson;

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());

	// cmd
	switch (motionType)
	{
	case EMotionType::Trajectory:
		root->SetStringField(TEXT("cmd"), TEXT("trajectoryAnalysis"));
		break;
	case EMotionType::Cpr:
		root->SetStringField(TEXT("cmd"), TEXT("cprAnalysis"));
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("StartCommand: unknown motion type %d"), (int32)motionType);
		root->SetStringField(TEXT("cmd"), TEXT("unknown"));
		break;
	}

	// action
	root->SetStringField(TEXT("action"), TEXT("begin"));

	// stamp: 毫秒时间戳
	const int64 TimestampMs = static_cast<int64>((FDateTime::UtcNow() - FDateTime(1970,1,1)).GetTotalMilliseconds());
	root->SetNumberField(TEXT("stamp"), (double)TimestampMs);

	cachedJson.Empty();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), Writer);
	return cachedJson;
}

const FString& CommandBuilder::EndCommand(EMotionType motionType)
{
	static FString cachedJson;

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());
	UCommandResolver* resolver = UCommandResolver::GetInstance();

	// cmd, bizId
	switch (motionType)
	{
	case EMotionType::Trajectory:
		root->SetStringField(TEXT("cmd"), TEXT("trajectoryAnalysis"));
		root->SetStringField(TEXT("track"), resolver->Get(TEXT("")));
		break;
	case EMotionType::Cpr:
		root->SetStringField(TEXT("cmd"), TEXT("cprAnalysis"));
		root->SetStringField(TEXT("cpr"), resolver->Get(TEXT("")));
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("EndCommand: unknown motion type %d"), (int32)motionType);
		root->SetStringField(TEXT("cmd"), TEXT("unknown"));
		break;
	}

	// action
	root->SetStringField(TEXT("action"), TEXT("stop"));

	// stamp
	const int64 TimestampMs = static_cast<int64>((FDateTime::UtcNow() - FDateTime(1970,1,1)).GetTotalMilliseconds());
	root->SetNumberField(TEXT("stamp"), (double)TimestampMs);

	cachedJson.Empty();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), Writer);
	return cachedJson;
}

const FString& CommandBuilder::AnalysisCommand(EMotionType motionType)
{
	static FString cachedJson;

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());
	UCommandResolver* resolver = UCommandResolver::GetInstance();

	// cmd, bizId
	switch (motionType)
	{
	case EMotionType::Trajectory:
		root->SetStringField(TEXT("cmd"), TEXT("trajectoryAnalysis"));
		root->SetStringField(TEXT("track"), resolver->Get(TEXT("")));
		break;
	case EMotionType::Cpr:
		root->SetStringField(TEXT("cmd"), TEXT("cprAnalysis"));
		root->SetStringField(TEXT("cpr"), resolver->Get(TEXT("")));
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("EndCommand: unknown motion type %d"), (int32)motionType);
		root->SetStringField(TEXT("cmd"), TEXT("unknown"));
		break;
	}

	// action
	root->SetStringField(TEXT("action"), TEXT("result"));

	cachedJson.Empty();
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), Writer);
	return cachedJson;
}

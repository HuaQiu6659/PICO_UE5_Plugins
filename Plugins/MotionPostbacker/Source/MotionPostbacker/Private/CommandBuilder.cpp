// Fill out your copyright notice in the Description page of Project Settings.


#include "CommandBuilder.h"
#include "CommandResolver.h"
#include "Json.h"
#include "JsonUtilities.h"

const FString& CommandBuilder::GlobalConfigCommand(const FString& clipperSn, const FString& dummySn)
{
	static FString cachedJson;
	cachedJson.Empty();

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());
	root->SetStringField(TEXT("cmd"), TEXT("rescueAppConfig"));
	root->SetNumberField(TEXT("fps"), 60);
	root->SetNumberField(TEXT("engine"), 1);
	root->SetStringField(TEXT("asepticClipper"), clipperSn);
	root->SetStringField(TEXT("dummy"), dummySn);

	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), Writer);
	return cachedJson;
}

const FString& CommandBuilder::StartCommand(EMotionType motionType)
{
	static FString cachedJson;
	cachedJson.Empty();

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
	case EMotionType::ZShape:
		root->SetStringField(TEXT("cmd"), TEXT("zshapeTrajectoryAnalysis"));
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("StartCommand: unknown motion type %d"), (int32)motionType);
		root->SetStringField(TEXT("cmd"), TEXT("unknown"));
		return cachedJson;
	}

	// action
	root->SetStringField(TEXT("action"), TEXT("begin"));

	// stamp: 毫秒时间戳
	const int64 timestampMs = static_cast<int64>((FDateTime::UtcNow() - FDateTime(1970,1,1)).GetTotalMilliseconds());
	root->SetNumberField(TEXT("stamp"), (double)timestampMs);

	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), writer);
	return cachedJson;
}

const FString& CommandBuilder::EndCommand(EMotionType motionType)
{
	static FString cachedJson;

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());
	UCommandResolver* resolver = UCommandResolver::GetInstance();
	const FString bizId = resolver ? resolver->GetBizId() : FString();
	if (bizId.IsEmpty())
	{
		cachedJson.Empty();
		return cachedJson;
	}
	root->SetStringField(TEXT("bizId"), bizId);

	// cmd, bizId
	switch (motionType)
	{
		case EMotionType::Trajectory:
			root->SetStringField(TEXT("cmd"), TEXT("trajectoryAnalysis"));
			break;
		case EMotionType::Cpr:
			root->SetStringField(TEXT("cmd"), TEXT("cprAnalysis"));
			break;
		case EMotionType::ZShape:
			root->SetStringField(TEXT("cmd"), TEXT("zshapeTrajectoryAnalysis"));
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
	cachedJson.Empty();

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());
	UCommandResolver* resolver = UCommandResolver::GetInstance();
	const FString bizId = resolver ? resolver->GetBizId() : FString();
	if (bizId.IsEmpty())
		return cachedJson;

	root->SetStringField(TEXT("bizId"), bizId);

	// cmd, bizId
	switch (motionType)
	{
		case EMotionType::Trajectory:
			root->SetStringField(TEXT("cmd"), TEXT("trajectoryAnalysis"));
			break;
		case EMotionType::Cpr:
			root->SetStringField(TEXT("cmd"), TEXT("cprAnalysis"));
			break;
		case EMotionType::ZShape:
			root->SetStringField(TEXT("cmd"), TEXT("zshapeTrajefctoryAnalysis"));
			break;
		default:
			return cachedJson;
	}

	// action
	root->SetStringField(TEXT("action"), TEXT("result"));

	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), writer);
	return cachedJson;
}

const FString& CommandBuilder::TrackerDatas(const TArray<FTrackerData>& trackers)
{
	static FString cachedJson;
	cachedJson.Empty();

	TSharedPtr<FJsonObject> root = MakeShareable(new FJsonObject());
	auto mode = UCommandResolver::GetInstance()->GetCurrentMode();
#if WITH_EDITOR	
	root->SetStringField(TEXT("cmd"), TEXT("trajectoryAnalysis"));
	root->SetStringField(TEXT("bizId"), TEXT("EDITOR_TEST"));
#else
	switch (mode)
	{
	case EMotionType::Trajectory:
		root->SetStringField(TEXT("cmd"), TEXT("trajectoryAnalysis"));
		break;

	case EMotionType::ZShape:
		root->SetStringField(TEXT("cmd"), TEXT("zshapeTrajectoryAnalysis"));
		break;

	default:
		return cachedJson;
	}
	UCommandResolver* resolver = UCommandResolver::GetInstance();
	const FString bizId = resolver ? resolver->GetBizId() : FString();
	if (bizId.IsEmpty())
		return cachedJson;
	root->SetStringField(TEXT("bizId"), bizId);
#endif

	root->SetStringField(TEXT("action"), TEXT("trReport"));
	const int64 TimestampMs = static_cast<int64>((FDateTime::UtcNow() - FDateTime(1970,1,1)).GetTotalMilliseconds());
	root->SetNumberField(TEXT("stamp"), (double)TimestampMs);

	TArray<TSharedPtr<FJsonValue>> trackersArray;
	trackersArray.Reserve(trackers.Num());

	for (const FTrackerData& t : trackers)
	{
		TSharedPtr<FJsonObject> tObj = MakeShareable(new FJsonObject());
		tObj->SetStringField(TEXT("sn"), t.sn);

		// lt: [x, y, z]
		{
			TArray<TSharedPtr<FJsonValue>> ltArr;
			ltArr.Add(MakeShareable(new FJsonValueNumber(t.lt.X)));
			ltArr.Add(MakeShareable(new FJsonValueNumber(t.lt.Y)));
			ltArr.Add(MakeShareable(new FJsonValueNumber(t.lt.Z)));
			tObj->SetArrayField(TEXT("lt"), ltArr);
		}

		// lr: [x, y, z, w]
		{
			TArray<TSharedPtr<FJsonValue>> lrArr;
			lrArr.Add(MakeShareable(new FJsonValueNumber(t.lr.X)));
			lrArr.Add(MakeShareable(new FJsonValueNumber(t.lr.Y)));
			lrArr.Add(MakeShareable(new FJsonValueNumber(t.lr.Z)));
			lrArr.Add(MakeShareable(new FJsonValueNumber(t.lr.W)));
			tObj->SetArrayField(TEXT("lr"), lrArr);
		}

		// gt: [x, y, z]
		{
			TArray<TSharedPtr<FJsonValue>> gtArr;
			gtArr.Add(MakeShareable(new FJsonValueNumber(t.gt.X)));
			gtArr.Add(MakeShareable(new FJsonValueNumber(t.gt.Y)));
			gtArr.Add(MakeShareable(new FJsonValueNumber(t.gt.Z)));
			tObj->SetArrayField(TEXT("gt"), gtArr);
		}

		// gr: [x, y, z, w]
		{
			TArray<TSharedPtr<FJsonValue>> grArr;
			grArr.Add(MakeShareable(new FJsonValueNumber(t.gr.X)));
			grArr.Add(MakeShareable(new FJsonValueNumber(t.gr.Y)));
			grArr.Add(MakeShareable(new FJsonValueNumber(t.gr.Z)));
			grArr.Add(MakeShareable(new FJsonValueNumber(t.gr.W)));
			tObj->SetArrayField(TEXT("gr"), grArr);
		}

		tObj->SetBoolField(TEXT("isConfidence"), t.bIsConfidence);

		trackersArray.Add(MakeShareable(new FJsonValueObject(tObj)));
	}

	root->SetArrayField(TEXT("trackerList"), trackersArray);
	TSharedRef<TJsonWriter<>> writer = TJsonWriterFactory<>::Create(&cachedJson);
	FJsonSerializer::Serialize(root.ToSharedRef(), writer);
	return cachedJson;
}
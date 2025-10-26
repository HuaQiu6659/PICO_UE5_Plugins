// Fill out your copyright notice in the Description page of Project Settings.
#include "CommandResolver.h"
#include "UObject/Package.h"
#include "Json.h"
#include "JsonUtilities.h"
#include "Misc/ScopeLock.h"

UCommandResolver* UCommandResolver::Instance = nullptr;

UCommandResolver* UCommandResolver::GetInstance()
{
    if (!Instance || !IsValid(Instance))
    {
        Instance = NewObject<UCommandResolver>(GetTransientPackage(), UCommandResolver::StaticClass());
        Instance->AddToRoot();
        Instance->Initialize();
    }
    return Instance;
}

void UCommandResolver::Initialize()
{
    idMap.Empty();
}
void UCommandResolver::Set(const FString& key, const FString& value) 
{
    FScopeLock lock(&idMapMutex);
    idMap.Add(key, value);
}

FString UCommandResolver::Get(const FString& key)
{
    FScopeLock lock(&idMapMutex);
    const FString* found = idMap.Find(key);
    return found ? *found : FString();
}

void UCommandResolver::Resolve(const FString& json)
{
    TSharedPtr<FJsonObject> jsonObject;
    TSharedRef<TJsonReader<TCHAR>> reader = TJsonReaderFactory<TCHAR>::Create(json);
    if (!FJsonSerializer::Deserialize(reader, jsonObject) || !jsonObject.IsValid())
    {
        FString err = FString::Printf(TEXT("Resolve: 无法解析为合法的 JSON: %s"), *json);
        onMotionUpdate.Broadcast(err);
        return;
    }

    const FString cmd = jsonObject->GetStringField(TEXT("cmd"));

    if (cmd.Equals(TEXT("onTrajectoryAnalysis"), ESearchCase::IgnoreCase))
    {
        OnTrajectoryAnalysis(jsonObject);
        return;
    }

    if (cmd.Equals(TEXT("onCprAnalysis"), ESearchCase::IgnoreCase))
    {
        OnCprAnalysis(jsonObject);
        return;
    }

    if (cmd.Equals(TEXT("onRescueAppConfig"), ESearchCase::IgnoreCase))
        OnRescueAppConfig(jsonObject);
}

void UCommandResolver::OnRescueAppConfig(const TSharedPtr<FJsonObject>& json)
{
    const int32 code = json->HasField(TEXT("code")) ? (int32)json->GetNumberField(TEXT("code")) : 0;
    const FString msg = json->HasField(TEXT("msg")) ? json->GetStringField(TEXT("msg")) : TEXT("");

    FString uiText;
    if (code == SUCCESS_CODE)
        uiText = TEXT("配置成功");
    else
        uiText = FString::Printf(TEXT("配置失败, %s"), *msg);
    UE_LOG(LogTemp, Log, TEXT("%s"), *uiText);
    onMotionUpdate.Broadcast(uiText);
}

// -------------------------- Trajectory --------------------------
void UCommandResolver::OnTrajectoryAnalysis(const TSharedPtr<FJsonObject>& json)
{
    const int32 code = json->HasField(TEXT("code")) ? (int32)json->GetNumberField(TEXT("code")) : 0;
    const FString msg = json->HasField(TEXT("msg")) ? json->GetStringField(TEXT("msg")) : TEXT("");
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    FString action;
    if (json->TryGetObjectField(TEXT("data"), dataPtr) && dataPtr && dataPtr->IsValid())
        action = (*dataPtr)->HasField(TEXT("action")) ? (*dataPtr)->GetStringField(TEXT("action")) : TEXT("");

    if (code != SUCCESS_CODE)
    {
        FString result = FString::Printf(TEXT("无菌钳\n错误码: %d\nAction: %s\nErr: %s"), code, *action, *msg);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *result);
        onMotionUpdate.Broadcast(result);
        return;
    }

    if (action.Equals(TEXT("begin"), ESearchCase::IgnoreCase))
        OnTrajectoryAnalysis_Begin(json);
    else if (action.Equals(TEXT("stop"), ESearchCase::IgnoreCase))
        OnTrajectoryAnalysis_Stop(json);
    else if (action.Equals(TEXT("trReport"), ESearchCase::IgnoreCase))
        OnTrajectoryAnalysis_TrReport(json);
    else if (action.Equals(TEXT("result"), ESearchCase::IgnoreCase))
        OnTrajectoryAnalysis_Result(json);
    else
    {
        const FString warn = FString::Printf(TEXT("无菌钳\n未知子指令: %s"), *action);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
    }
}

void UCommandResolver::OnTrajectoryAnalysis_Begin(const TSharedPtr<FJsonObject>& json)
{
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    if (!json.IsValid() || !json->TryGetObjectField(TEXT("data"), dataPtr) || !(dataPtr && dataPtr->IsValid()))
    {
        const FString warn = TEXT("OnTrajectoryAnalysis_Begin: invalid data");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }
    FString bizId;
    (*dataPtr)->TryGetStringField(TEXT("bizId"), bizId);
    Set(TEXT("track"), bizId);
    {
        const FString info = FString::Printf(TEXT("Trajectory Begin bizId=%s"), *bizId);
        UE_LOG(LogTemp, Log, TEXT("%s"), *info);
        onMotionUpdate.Broadcast(info);
    }
}

void UCommandResolver::OnTrajectoryAnalysis_Stop(const TSharedPtr<FJsonObject>& json)
{
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    if (!json.IsValid() || !json->TryGetObjectField(TEXT("data"), dataPtr) || !(dataPtr && dataPtr->IsValid()))
    {
        const FString warn = TEXT("OnTrajectoryAnalysis_Stop: invalid data");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("无菌钳轨迹分析: 已停止"));
    onMotionUpdate.Broadcast(TEXT("无菌钳轨迹分析: 已停止"));
}

void UCommandResolver::OnTrajectoryAnalysis_TrReport(const TSharedPtr<FJsonObject>& json)
{
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    if (!json.IsValid() || !json->TryGetObjectField(TEXT("data"), dataPtr) || !(dataPtr && dataPtr->IsValid()))
    {
        const FString warn = TEXT("OnTrajectoryAnalysis_TrReport: invalid data");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }

    //啥也不用干
}

void UCommandResolver::OnTrajectoryAnalysis_Result(const TSharedPtr<FJsonObject>& json)
{
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    if (!json.IsValid() || !json->TryGetObjectField(TEXT("data"), dataPtr) || !(dataPtr && dataPtr->IsValid()))
    {
        const FString warn = TEXT("OnTrajectoryAnalysis_Result: invalid data");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }
    // 读取 isFinish 与 summary 字段
    const bool isFinish = (*dataPtr)->HasField(TEXT("isFinish")) ? (*dataPtr)->GetBoolField(TEXT("isFinish")) : false;
    if (isFinish)
    {
        const TSharedPtr<FJsonObject>* summaryPtr = nullptr;
        if ((*dataPtr)->TryGetObjectField(TEXT("summary"), summaryPtr) && summaryPtr && summaryPtr->IsValid())
        {
            const bool is1cmFromInjurySite = (*summaryPtr)->HasField(TEXT("is1cmFromInjurySite")) ? (*summaryPtr)->GetBoolField(TEXT("is1cmFromInjurySite")) : false;
            const bool isSpiral = (*summaryPtr)->HasField(TEXT("isSpiral")) ? (*summaryPtr)->GetBoolField(TEXT("isSpiral")) : false;
            const bool isInOrder = (*summaryPtr)->HasField(TEXT("isInOrder")) ? (*summaryPtr)->GetBoolField(TEXT("isInOrder")) : false;
            const double sphereDiameter = (*summaryPtr)->HasField(TEXT("sphereDiameter")) ? (*summaryPtr)->GetNumberField(TEXT("sphereDiameter")) : 0.0;
            const double score = (*summaryPtr)->HasField(TEXT("score")) ? (*summaryPtr)->GetNumberField(TEXT("score")) : 0.0;
            
            FString result = FString::Printf(TEXT("避开穿刺点1cm: %s\n螺旋式消毒: %s\n方向和顺序: %s\n消毒直径: %.2f米\n得分: %.2f"),
                is1cmFromInjurySite ? TEXT("是") : TEXT("否"),
                isSpiral ? TEXT("是") : TEXT("否"),
                isInOrder ? TEXT("是") : TEXT("否"),
                sphereDiameter, score);

            UE_LOG(LogTemp, Log, TEXT("%s"), *result);
            onMotionUpdate.Broadcast(result);
        }
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("无菌钳\n轨迹分析: 未完成"));
        onMotionUpdate.Broadcast(TEXT("无菌钳\n轨迹分析: 未完成"));
    }
}

// -------------------------- CPR --------------------------
void UCommandResolver::OnCprAnalysis(const TSharedPtr<FJsonObject>& json)
{
    if (!json.IsValid())
    {
        const FString warn = TEXT("OnCprAnalysis: invalid json");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }
    int32 code = 0; FString msg; const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    json->TryGetNumberField(TEXT("code"), code);
    json->TryGetStringField(TEXT("msg"), msg);
    json->TryGetObjectField(TEXT("data"), dataPtr);

    FString action;
    if (dataPtr && dataPtr->IsValid()) { (*dataPtr)->TryGetStringField(TEXT("action"), action); }

    if (code == SUCCESS_CODE)
    {
        if (action.Equals(TEXT("begin"), ESearchCase::IgnoreCase)) 
            OnCprAnalysis_Begin(json);
        else if (action.Equals(TEXT("end"), ESearchCase::IgnoreCase)) 
            OnCprAnalysis_End(json);
        else if (action.Equals(TEXT("result"), ESearchCase::IgnoreCase))
            OnCprAnalysis_Result(json);
        else
        {
            const FString warn = FString::Printf(TEXT("OnCprAnalysis: unknown action=%s"), *action);
            UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
            onMotionUpdate.Broadcast(warn);
        }
    }
    else
    {
        const FString warn = FString::Printf(TEXT("OnCprAnalysis: code=%d, msg=%s, action=%s"), code, *msg, *action);
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
    }
}

void UCommandResolver::OnCprAnalysis_Begin(const TSharedPtr<FJsonObject>& json)
{
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    if (!json.IsValid() || !json->TryGetObjectField(TEXT("data"), dataPtr) || !(dataPtr && dataPtr->IsValid()))
    {
        const FString warn = TEXT("OnCprAnalysis_Begin: invalid data");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }
    FString bizId;
    (*dataPtr)->TryGetStringField(TEXT("bizId"), bizId);
    Set(TEXT("cpr"), bizId);
    {
        const FString info = FString::Printf(TEXT("CPR Begin bizId=%s"), *bizId);
        UE_LOG(LogTemp, Log, TEXT("%s"), *info);
        onMotionUpdate.Broadcast(info);
    }
}

void UCommandResolver::OnCprAnalysis_End(const TSharedPtr<FJsonObject>& json)
{
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    if (!json.IsValid() || !json->TryGetObjectField(TEXT("data"), dataPtr) || !(dataPtr && dataPtr->IsValid()))
    {
        const FString warn = TEXT("OnCprAnalysis_End: invalid data");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("CPR 分析: 已停止"));
    onMotionUpdate.Broadcast(TEXT("CPR 分析: 已停止"));
}

void UCommandResolver::OnCprAnalysis_Result(const TSharedPtr<FJsonObject>& json)
{
    const TSharedPtr<FJsonObject>* dataPtr = nullptr;
    if (!json.IsValid() || !json->TryGetObjectField(TEXT("data"), dataPtr) || !(dataPtr && dataPtr->IsValid()))
    {
        const FString warn = TEXT("OnCprAnalysis_Result: invalid data");
        UE_LOG(LogTemp, Warning, TEXT("%s"), *warn);
        onMotionUpdate.Broadcast(warn);
        return;
    }

    const bool isFinish = (*dataPtr)->HasField(TEXT("isFinish")) ? (*dataPtr)->GetBoolField(TEXT("isFinish")) : false;
    const TSharedPtr<FJsonObject>* summaryPtr = nullptr;
    if ((*dataPtr)->TryGetObjectField(TEXT("summary"), summaryPtr) && summaryPtr && summaryPtr->IsValid())
    {
        const bool isArmsStraight = (*summaryPtr)->HasField(TEXT("isArmsStraight")) ? (*summaryPtr)->GetBoolField(TEXT("isArmsStraight")) : false;
        const bool isPerpendicular = (*summaryPtr)->HasField(TEXT("isPerpendicular")) ? (*summaryPtr)->GetBoolField(TEXT("isPerpendicular")) : false;
        const double scoreNum = (*summaryPtr)->HasField(TEXT("score")) ? (*summaryPtr)->GetNumberField(TEXT("score")) : 0.0;

        FString result = FString::Printf(TEXT("CPR 结果: \n手臂是否伸直: %s\n按压是否垂直: %s\n得分: %.2f"),
            isArmsStraight ? TEXT("是") : TEXT("否"),
            isPerpendicular ? TEXT("是") : TEXT("否"),
            scoreNum);

        UE_LOG(LogTemp, Log, TEXT("%s"), *result);
        onMotionUpdate.Broadcast(result);
    }

    const FString infoFinish = FString::Printf(TEXT("OnCprAnalysis_Result: isFinish=%s"), isFinish ? TEXT("true") : TEXT("false"));
    UE_LOG(LogTemp, Log, TEXT("%s"), *infoFinish);
    onMotionUpdate.Broadcast(infoFinish);
}

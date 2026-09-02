#include "pch.h"
#include "ModelRenderPoolComponent.h"

#include "GameObject/GameObject.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Render/MaterialComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <string>

using namespace CoreEngine;

void GameComponents::ModelRenderPoolComponent::Awake() {
    if (modelPath_.empty()) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "ModelRenderPoolComponent: モデルパスが空です");
        SetEnabled(false);
        return;
    }

    entries_.reserve(initialCapacity_);
    while (entries_.size() < initialCapacity_) {
        if (!CreateEntry()) {
            Logger::GetInstance().Errorf(
                LogCategory::Game,
                "ModelRenderPoolComponent: プールの事前生成に失敗しました ({}/{})",
                entries_.size(), initialCapacity_);
            SetEnabled(false);
            return;
        }
    }
}

void GameComponents::ModelRenderPoolComponent::Update() {
    const std::uint64_t frame = Time::FrameCount();
    for (Entry& entry : entries_) {
        if (!entry.object || entry.object->IsMarkedForDestroy()) {
            continue;
        }

        // 呼び出し元がこの Update より先に Draw() していても、同じフレームなら残す。
        if (entry.lastSubmittedFrame != frame && entry.object->IsActive()) {
            entry.object->SetActive(false);
        }
    }
}

void GameComponents::ModelRenderPoolComponent::OnDestroy() {
    for (Entry& entry : entries_) {
        if (entry.object && !entry.object->IsMarkedForDestroy()) {
            entry.object->Destroy();
        }
    }
    entries_.clear();
}

bool GameComponents::ModelRenderPoolComponent::Draw(
    const Vector3& position,
    const Vector3& rotation,
    const Vector3& scale) {
    const std::uint64_t frame = Time::FrameCount();
    Entry* entry = FindAvailableEntry(frame);
    if (!entry && allowGrowth_) {
        entry = CreateEntry();
    }

    if (!entry || !entry->object || !entry->transform) {
        // 固定容量を超えた場合でも、警告は1フレームに1回までに抑える。
        if (lastExhaustedWarningFrame_ != frame) {
            lastExhaustedWarningFrame_ = frame;
            Logger::GetInstance().Warnf(
                LogCategory::Game,
                "ModelRenderPoolComponent: 1フレームの描画数が容量を超えました (capacity={})",
                entries_.size());
        }
        return false;
    }

    auto& transform = entry->transform->Get();
    transform.translate = position;
    transform.rotate = rotation;
    transform.scale = scale;
    transform.TransferMatrix();

    entry->lastSubmittedFrame = frame;
    entry->object->SetActive(true);
    return true;
}

std::size_t GameComponents::ModelRenderPoolComponent::GetActiveCount() const {
    const std::uint64_t frame = Time::FrameCount();
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(), entries_.end(),
        [frame](const Entry& entry) {
            return entry.lastSubmittedFrame == frame;
        }));
}

GameComponents::ModelRenderPoolComponent::Entry*
GameComponents::ModelRenderPoolComponent::CreateEntry() {
    GameObject* owner = GetOwner();
    if (!owner) {
        return nullptr;
    }

    GameObject* object = owner->Spawn<GameObject>();
    if (!object) {
        return nullptr;
    }

    object->SetName(
        owner->GetName() + "_PooledModel_" + std::to_string(entries_.size()));
    object->SetSerializeEnabled(false);

    TransformComponent* transform = object->AddComponent<TransformComponent>();
    object->AddComponent<MeshRendererComponent>(modelPath_);
    if (color_) {
        auto* material = object->AddComponent<MaterialComponent>();
        material->SetColor(*color_);
        material->SetPBR(0.0f, 0.15f);
    }
    object->SetActive(false);

    entries_.push_back({ object, transform });
    return &entries_.back();
}

GameComponents::ModelRenderPoolComponent::Entry*
GameComponents::ModelRenderPoolComponent::FindAvailableEntry(std::uint64_t frame) {
    if (allocationFrame_ != frame) {
        allocationFrame_ = frame;
        nextEntryIndex_ = 0;
    }

    while (nextEntryIndex_ < entries_.size()) {
        Entry& entry = entries_[nextEntryIndex_++];
        if (entry.lastSubmittedFrame != frame && entry.object &&
            !entry.object->IsMarkedForDestroy()) {
            return &entry;
        }
    }
    return nullptr;
}

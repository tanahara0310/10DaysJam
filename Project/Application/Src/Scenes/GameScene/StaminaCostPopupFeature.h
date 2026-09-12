#pragma once

#include "Scene/Feature/ISceneFeature.h"

#include <memory>

namespace GameComponents
{
    /// @brief 消費スタミナを線路先端付近に表示し、上昇しながらフェードアウトする。
    std::unique_ptr<CoreEngine::ISceneFeature> CreateStaminaCostPopupFeature();
}

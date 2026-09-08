#pragma once

#include "Scene/Feature/ISceneFeature.h"

#include <memory>

namespace GameComponents
{
    // リザルトのサルを注視し、周回・手振れ・接近を順番に繰り返す。
    std::unique_ptr<CoreEngine::ISceneFeature> CreateResultCameraFeature();
}

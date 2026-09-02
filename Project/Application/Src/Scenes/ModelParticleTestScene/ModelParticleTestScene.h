#pragma once

#include "Scene/BaseScene.h"

namespace CoreEngine {
    class ParticleSystem;
}

namespace ModelParticleTest
{
    /// @brief モデルパーティクルのライティング検証シーン
    /// @details
    ///  モデルパーティクルは長らくアンリット（テクスチャ色 × パーティクル色）だったため、
    ///  どの向きを向いても真っ平らに見えていた。ディレクショナルライトによる陰影を
    ///  入れたので、それが効いているかを目で確かめる。
    ///
    ///  見るのは 1 点だけ:
    ///   左の球（MeshRendererComponent = 既存の受光経路）と
    ///   右に並ぶ球（モデルパーティクル = 今回の経路）に、
    ///   **同じ向きの明暗が付いているか**。
    ///   パーティクル側が真っ白でのっぺりしていたらライティングが効いていない。
    ///
    ///  @note シェーディングモデルは左右で異なる（左が PBR、右がハーフランバート）ので、
    ///        明るさが厳密に一致することは期待しない。見るのは陰影の向きと有無。
    class ModelParticleTestScene : public CoreEngine::BaseScene {
    public:
        ModelParticleTestScene() = default;
        ~ModelParticleTestScene() override = default;

        void OnInitialize() override;

    private:
        /// @brief 比較元となる通常メッシュの球を置く（既存の受光経路）
        void CreateReferenceSphere();

        /// @brief 検証対象のモデルパーティクルを置く（今回の経路）
        void CreateModelParticles();

        /// 生成したパーティクルシステム（所有は GameObjectManager）
        CoreEngine::ParticleSystem* particleSystem_ = nullptr;
    };
}

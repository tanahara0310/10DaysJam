#pragma once

#include "Scene/BaseScene.h"

namespace CoreEngine {
    class ParticleSystem;
}

namespace ModelParticleTest
{
    /// @brief モデルパーティクルのライティング・影の検証シーン
    /// @details
    ///  中心から一定間隔で爆散する小さなモデルパーティクルを置く。見るのは 2 点。
    ///
    ///  1. **陰影が付いているか** — 左に置いた比較用の球（MeshRendererComponent =
    ///     既存の受光経路）と同じ向きの明暗が、粒にも出ているか。
    ///     粒が真っ白でのっぺりしていたらライティングが効いていない。
    ///  2. **地面に影が落ちているか** — 粒は TLAS へインスタンスとして登録されるので、
    ///     DXR のシャドウレイが拾って地面に影を落とす。
    ///
    ///  @note シェーディングモデルは比較球と粒で異なる（前者が PBR、後者がハーフランバート）
    ///        ため、明るさが厳密に一致することは期待しない。見るのは陰影の向きと有無。
    ///  @note 光の向きは大気設定（Sky Atmosphere パネル / CVar 保存値）が決めるので、
    ///        シーン側からは固定できない。影の向きを変えたい場合はそちらを操作する。
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

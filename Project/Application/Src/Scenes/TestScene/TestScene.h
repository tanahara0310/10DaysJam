#pragma once

#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <numbers>
#include <random>
#include <vector>
#include <memory>

// エンジンコア
#include "Audio/AudioSystem.h"
#include "Camera/CameraManager.h"
#include "Camera/Camera.h"
#include "Camera/Camera.h"
#include "Math/MathCore.h"
#include "Utility/Logger/Logger.h"
#include "Graphics/Texture/TextureManager.h"
#include "Graphics/Light/LightData.h"
#include "Graphics/Model/ModelManager.h"
#include "Graphics/Model/Model.h"
#include "Graphics/Line/LineManager.h"

// シーン関連
#include "Scene/BaseScene.h"
#include "EngineSystem/EngineSystem.h"

// GameObjectのインクルード

using namespace Microsoft::WRL;

/// @brief テストシーンクラス

namespace CoreEngine
{
    class TestScene : public BaseScene {
    public:
        /// @brief シーン固有の初期化
        void OnInitialize() override;

    protected:
        /// @brief 更新処理（BaseSceneのOnUpdate()をオーバーライド）
        void OnUpdate() override;

    private: // オーディオ動作確認

        /// @brief 音の読み込みと BGM の再生開始（結果はログに出す）
        void InitializeAudioTest();

        /// @brief キー入力でのオーディオ操作を受け付ける
        void UpdateAudioTest();

        /// @brief 現在のオーディオ状態をログへ書き出す
        void LogAudioState(const char* label) const;

        /// @brief 起動直後の数秒だけ走る自動確認シーケンス
        /// @details 人がキーを押さなくてもログだけで「重ね再生・フェード進行・
        ///          スロット自動回収・バス音量」が確認できるようにするためのもの。
        void RunAudioAutoCheck();

    private: // メンバ変数

        Logger& logger = Logger::GetInstance();

        AudioSystem* audio_ = nullptr;
        SoundClip seClip_;   ///< 単発 SE 用
        SoundClip bgmClip_;  ///< ループ BGM 用

        /// @brief シーンが所有する BGM。破棄されると自動で止まる
        ScopedSound bgm_;

        // 自動確認シーケンスの進行状態
        float audioCheckTimer_ = 0.0f;
        int audioCheckStep_ = 0;
    };
}

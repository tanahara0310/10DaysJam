#pragma once

#include "GameObject/Component/Core/IComponent.h"
#include "Utility/CVar/CVar.h"

namespace GameComponents
{
    /// @brief GameScene の生成時に各コンポーネントへ渡す調整値。
    /// @details CVarSettingsSection により CVars.json へ自動保存される。
    namespace GameSettings
    {
        extern CoreEngine::CVar<CoreEngine::Vector3> ReleaseCameraPosition;
        extern CoreEngine::CVar<CoreEngine::Vector3> ReleaseCameraRotation;
        extern CoreEngine::CVar<float> BgmVolume;

        extern CoreEngine::CVar<float> GridSize;
        extern CoreEngine::CVar<int> MapSizeZ;
        extern CoreEngine::CVar<int> InitialMapSizeX;
        extern CoreEngine::CVar<int> RenderDistance;
        extern CoreEngine::CVar<int> CsvChunkSizeX;

        extern CoreEngine::CVar<int> InitialRailResources;
        extern CoreEngine::CVar<int> BuilderStartX;
        extern CoreEngine::CVar<int> BuilderStartZ;
        extern CoreEngine::CVar<float> TrainMoveSpeed;

        extern CoreEngine::CVar<int> GroundPoolCapacity;
        extern CoreEngine::CVar<int> WaterPoolCapacity;
        extern CoreEngine::CVar<int> StationPoolCapacity;
        extern CoreEngine::CVar<int> RockPoolCapacity;
        extern CoreEngine::CVar<int> RailPoolCapacity;
        extern CoreEngine::CVar<int> RailLeftPoolCapacity;
        extern CoreEngine::CVar<int> RailRightPoolCapacity;

        extern CoreEngine::CVar<float> CameraFocusRatio;
        extern CoreEngine::CVar<CoreEngine::Vector3> CameraOffset;
        extern CoreEngine::CVar<float> CameraMinTargetDistance;
        extern CoreEngine::CVar<float> CameraMaxTargetDistance;
        extern CoreEngine::CVar<float> CameraMinFovDegrees;
        extern CoreEngine::CVar<float> CameraMaxFovDegrees;
        extern CoreEngine::CVar<float> CameraFollowSpeed;

        extern CoreEngine::CVar<CoreEngine::Vector2> HudPosition;
        extern CoreEngine::CVar<float> HudFontSize;
        extern CoreEngine::CVar<CoreEngine::Vector4> HudColor;
        extern CoreEngine::CVar<CoreEngine::Vector4> HudOutlineColor;
        extern CoreEngine::CVar<float> HudOutlineWidth;
        extern CoreEngine::CVar<int> HudSortOrder;
    }

    /// @brief GameScene の調整値をオブジェクトインスペクターへ表示する。
    class GameSettingsComponent final : public CoreEngine::IComponent
    {
    public:
        const char* GetTypeName() const override { return "GameSettings"; }

#ifdef USE_IMGUI
        const char* GetInspectorName() const override { return "ゲーム設定"; }
        const char* GetInspectorIcon() const override { return "scene.png"; }
        void GetInspectorIconColor(float* outRgba) const override
        {
            outRgba[0] = 0.35f;
            outRgba[1] = 0.85f;
            outRgba[2] = 0.45f;
            outRgba[3] = 1.0f;
        }
        bool DrawInspector() override;
#endif
    };
}

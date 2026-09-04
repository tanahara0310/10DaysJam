#include "pch.h"
#include "GameSettingsComponent.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/CVarPanel.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"
#endif

namespace GameComponents::GameSettings
{
    CoreEngine::CVar<CoreEngine::Vector3> ReleaseCameraPosition{
        "Game.Scene.ReleaseCameraPosition", { 0.0f, 2.0f, 0.0f },
        "リリース用カメラの初期位置",
        CoreEngine::CVarRange{ -100.0f, 100.0f } };
    CoreEngine::CVar<CoreEngine::Vector3> ReleaseCameraRotation{
        "Game.Scene.ReleaseCameraRotation", { 0.3f, 0.0f, 0.0f },
        "リリース用カメラの初期回転（ラジアン）",
        CoreEngine::CVarRange{ -6.283185f, 6.283185f } };
    CoreEngine::CVar<float> BgmVolume{
        "Game.Scene.BgmVolume", 1.0f / 3.0f,
        "ゲームBGMの音量", CoreEngine::CVarRange{ 0.0f, 1.0f } };

    CoreEngine::CVar<float> GridSize{
        "Game.World.GridSize", 1.0f,
        "マップ・レール・列車で共有する1マスの大きさ",
        CoreEngine::CVarRange{ 0.1f, 10.0f } };
    CoreEngine::CVar<int> MapSizeZ{
        "Game.World.MapSizeZ", 9,
        "マップのZ方向のマス数", CoreEngine::CVarRange{ 1.0f, 100.0f } };
    CoreEngine::CVar<int> InitialMapSizeX{
        "Game.World.InitialMapSizeX", 30,
        "シーン開始時に生成するX方向のマス数",
        CoreEngine::CVarRange{ 1.0f, 500.0f } };
    CoreEngine::CVar<int> RenderDistance{
        "Game.World.RenderDistance", 20,
        "カメラ注視点の前後に描画するX方向の距離",
        CoreEngine::CVarRange{ 1.0f, 200.0f } };
    CoreEngine::CVar<int> CsvChunkSizeX{
        "Game.World.CsvChunkSizeX", 10,
        "ランダムCSV区画のX方向の幅",
        CoreEngine::CVarRange{ 1.0f, 200.0f } };

    CoreEngine::CVar<int> InitialRailResources{
        "Game.Rail.InitialResources", 15,
        "ゲーム開始時に所持するレール数",
        CoreEngine::CVarRange{ 0.0f, 999.0f } };
    CoreEngine::CVar<int> BuilderStartX{
        "Game.Rail.BuilderStartX", 3,
        "レールビルダーと列車の開始X座標",
        CoreEngine::CVarRange{ 0.0f, 500.0f } };
    CoreEngine::CVar<int> BuilderStartZ{
        "Game.Rail.BuilderStartZ", 4,
        "レールビルダーと列車の開始Z座標",
        CoreEngine::CVarRange{ 0.0f, 99.0f } };
    CoreEngine::CVar<float> TrainMoveSpeed{
        "Game.Rail.TrainMoveSpeed", 0.5f,
        "列車の初期移動速度（マス/秒）",
        CoreEngine::CVarRange{ 0.01f, 10.0f } };

    CoreEngine::CVar<int> GroundPoolCapacity{
        "Game.Pools.GroundCapacity", 600, "床モデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };
    CoreEngine::CVar<int> WaterPoolCapacity{
        "Game.Pools.WaterCapacity", 100, "水モデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };
    CoreEngine::CVar<int> StationPoolCapacity{
        "Game.Pools.StationCapacity", 10, "駅モデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 1000.0f } };
    CoreEngine::CVar<int> RockPoolCapacity{
        "Game.Pools.RockCapacity", 50, "岩モデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 1000.0f } };
    CoreEngine::CVar<int> RailPoolCapacity{
        "Game.Pools.RailCapacity", 100, "直線レールモデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };
    CoreEngine::CVar<int> RailLeftPoolCapacity{
        "Game.Pools.RailLeftCapacity", 50, "左カーブレールモデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };
    CoreEngine::CVar<int> RailRightPoolCapacity{
        "Game.Pools.RailRightCapacity", 50, "右カーブレールモデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };

    CoreEngine::CVar<float> CameraFocusRatio{
        "Game.Camera.FocusRatio", 0.8f,
        "カメラX位置の寄せ具合（0=列車、1=ビルダー）",
        CoreEngine::CVarRange{ 0.0f, 1.0f } };
    CoreEngine::CVar<CoreEngine::Vector3> CameraOffset{
        "Game.Camera.Offset", { 0.0f, 20.0f, -18.0f },
        "注視点からカメラまでのオフセット",
        CoreEngine::CVarRange{ -100.0f, 100.0f } };
    CoreEngine::CVar<float> CameraMinTargetDistance{
        "Game.Camera.MinTargetDistance", 5.0f,
        "最小FOVを使う列車・ビルダー間距離",
        CoreEngine::CVarRange{ 0.0f, 100.0f } };
    CoreEngine::CVar<float> CameraMaxTargetDistance{
        "Game.Camera.MaxTargetDistance", 30.0f,
        "最大FOVを使う列車・ビルダー間距離",
        CoreEngine::CVarRange{ 0.1f, 300.0f } };
    CoreEngine::CVar<float> CameraMinFovDegrees{
        "Game.Camera.MinFovDegrees", 35.0f,
        "列車とビルダーが近いときの視野角",
        CoreEngine::CVarRange{ 1.0f, 179.0f } };
    CoreEngine::CVar<float> CameraMaxFovDegrees{
        "Game.Camera.MaxFovDegrees", 70.0f,
        "列車とビルダーが遠いときの視野角",
        CoreEngine::CVarRange{ 1.0f, 179.0f } };
    CoreEngine::CVar<float> CameraFollowSpeed{
        "Game.Camera.FollowSpeed", 3.0f,
        "カメラ位置・注視点・FOVの追従速度",
        CoreEngine::CVarRange{ 0.01f, 30.0f } };

    CoreEngine::CVar<CoreEngine::Vector2> HudPosition{
        "Game.Hud.Position", { 32.0f, 32.0f },
        "画面左上を基準にしたレール数表示位置",
        CoreEngine::CVarRange{ -2000.0f, 2000.0f } };
    CoreEngine::CVar<float> HudFontSize{
        "Game.Hud.FontSize", 108.0f,
        "レール数表示のフォントサイズ",
        CoreEngine::CVarRange{ 8.0f, 256.0f } };
    CoreEngine::CVar<CoreEngine::Vector4> HudColor{
        "Game.Hud.Color", { 1.0f, 1.0f, 1.0f, 1.0f },
        "レール数表示の文字色（RGBA）" };
    CoreEngine::CVar<CoreEngine::Vector4> HudOutlineColor{
        "Game.Hud.OutlineColor", { 0.0f, 0.0f, 0.0f, 1.0f },
        "レール数表示のアウトライン色（RGBA）" };
    CoreEngine::CVar<float> HudOutlineWidth{
        "Game.Hud.OutlineWidth", 0.035f,
        "レール数表示のアウトライン幅",
        CoreEngine::CVarRange{ 0.0f, 0.25f } };
    CoreEngine::CVar<int> HudSortOrder{
        "Game.Hud.SortOrder", 1000,
        "レール数表示の描画順",
        CoreEngine::CVarRange{ 0.0f, 5000.0f } };
}

#ifdef USE_IMGUI
bool GameComponents::GameSettingsComponent::DrawInspector()
{
    const bool changed = CoreEngine::CVarUI::DrawTree("Game");

    CoreEngine::UI::Separator();
    CoreEngine::UI::Hint(
        "変更はCVars.jsonへ自動保存されます。"
        "ゲームシーンを再読み込みすると各コンポーネントへ反映されます。");
    return changed;
}
#endif

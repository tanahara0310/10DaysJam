#include "pch.h"
#include "GameSettingsComponent.h"

#ifdef USE_IMGUI
#include "Editor/ImGui/CVarPanel.h"
#include "Editor/ImGui/Wrappers/ImGuiLayout.h"
#endif

namespace GameComponents::GameSettings
{
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
    CoreEngine::CVar<int> BananaTreePoolCapacity{
        "Game.Pools.BananaTreeCapacity", 50, "バナナの木モデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 1000.0f } };
    CoreEngine::CVar<int> BridgePoolCapacity{
        "Game.Pools.BridgeCapacity", 100, "橋モデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };
    CoreEngine::CVar<int> RailPoolCapacity{
        "Game.Pools.RailCapacity", 100, "直線レールモデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };
    CoreEngine::CVar<int> RailLeftPoolCapacity{
        "Game.Pools.RailLeftCapacity", 50, "左カーブレールモデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };
    CoreEngine::CVar<int> RailRightPoolCapacity{
        "Game.Pools.RailRightCapacity", 50, "右カーブレールモデルの初期プール数",
        CoreEngine::CVarRange{ 1.0f, 5000.0f } };

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

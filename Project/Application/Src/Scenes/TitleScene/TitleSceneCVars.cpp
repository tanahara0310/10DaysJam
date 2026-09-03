#include "pch.h"
#include "TitleSceneCVars.h"

namespace TitleSceneCVars
{
    // ===== title.obj の最終姿勢 =====
    // 位置・回転・スケールは TransformComponent へ初期値として渡す。
    // 回転は既存のエンジン規約に合わせてラジアンで指定する。
    CoreEngine::CVar<CoreEngine::Vector3> Position{
        "Title.Transform.Position",
        { -0.08570337f, 3.187f, -0.81899166f },
        "title.obj の最終位置（ワールド座標）",
        CoreEngine::CVarRange{ -100.0f, 100.0f } };

    CoreEngine::CVar<CoreEngine::Vector3> Rotation{
        "Title.Transform.Rotation",
        { 0.0f, 0.0f, 0.0f },
        "title.obj の最終回転（ラジアン）",
        CoreEngine::CVarRange{ -6.283185f, 6.283185f } };

    CoreEngine::CVar<CoreEngine::Vector3> Scale{
        "Title.Transform.Scale",
        { 1.0f, 1.0f, 1.0f },
        "title.obj の最終スケール",
        CoreEngine::CVarRange{ 0.01f, 10.0f } };

    // ===== ロゴの登場・待機アニメーション =====
    CoreEngine::CVar<float> IntroDuration{
        "Title.Animation.IntroDuration",
        1.05f,
        "ロゴが落下して着地するまでの時間（秒）",
        CoreEngine::CVarRange{ 0.05f, 5.0f } };

    CoreEngine::CVar<float> IntroScale{
        "Title.Animation.IntroScale",
        0.82f,
        "落下開始時のロゴのスケール（最終スケールに対する倍率）",
        CoreEngine::CVarRange{ 0.1f, 1.0f } };

    CoreEngine::CVar<float> DropHeight{
        "Title.Animation.DropHeight",
        3.5f,
        "ロゴを最終位置より上へ離す高さ（メートル）",
        CoreEngine::CVarRange{ 0.0f, 20.0f } };

    CoreEngine::CVar<float> BobHeight{
        "Title.Animation.BobHeight",
        0.12f,
        "着地後に上下へ浮遊する高さ（メートル）",
        CoreEngine::CVarRange{ 0.0f, 2.0f } };

    CoreEngine::CVar<float> BobDuration{
        "Title.Animation.BobDuration",
        1.6f,
        "着地後の上下浮遊が片道にかかる時間（秒）",
        CoreEngine::CVarRange{ 0.1f, 10.0f } };

    CoreEngine::CVar<float> RotationAmplitude{
        "Title.Animation.RotationAmplitude",
        0.035f,
        "落下時・待機時の左右回転幅（ラジアン）",
        CoreEngine::CVarRange{ 0.0f, 1.0f } };

    CoreEngine::CVar<float> ShakeStrength{
        "Title.CameraShake.Strength",
        1.0f,
        "タイトルモデルのバウンド時カメラシェイク強度（0で無効、1で標準）",
        CoreEngine::CVarRange{ 0.0f, 2.0f } };

    // ===== トロッコ登場演出 =====
    CoreEngine::CVar<CoreEngine::Vector3> TrolleyPosition{
        "Title.Trolley.Position",
        { 0.0f, 0.0f, -0.5f },
        "トロッコの最終位置（ワールド座標）",
        CoreEngine::CVarRange{ -100.0f, 100.0f } };

    CoreEngine::CVar<float> TrolleyIntroDelay{
        "Title.Trolley.IntroDelay",
        0.35f,
        "タイトルの落下完了後、トロッコが登場するまでの待ち時間（秒）",
        CoreEngine::CVarRange{ 0.0f, 5.0f } };

    CoreEngine::CVar<float> TrolleyIntroDuration{
        "Title.Trolley.IntroDuration",
        0.9f,
        "トロッコが画面下から登場する時間（秒）",
        CoreEngine::CVarRange{ 0.05f, 5.0f } };

    CoreEngine::CVar<float> TrolleyIntroOffset{
        "Title.Trolley.IntroOffset",
        8.0f,
        "トロッコを最終位置より下へ離す距離（メートル）",
        CoreEngine::CVarRange{ 0.0f, 30.0f } };

    CoreEngine::CVar<CoreEngine::Vector3> TrolleyBobStart{
        "Title.Trolley.BobStart",
        { 0.0f, 0.0f, 0.0f },
        "浮遊の線形補間を開始する位置オフセット（トロッコ基準）",
        CoreEngine::CVarRange{ -10.0f, 10.0f } };

    CoreEngine::CVar<CoreEngine::Vector3> TrolleyBobEnd{
        "Title.Trolley.BobEnd",
        { 0.0f, 0.12f, 0.0f },
        "浮遊の線形補間を終了する位置オフセット（トロッコ基準）",
        CoreEngine::CVarRange{ -10.0f, 10.0f } };

    CoreEngine::CVar<float> TrolleyBobDuration{
        "Title.Trolley.BobDuration",
        1.6f,
        "トロッコが上下へ浮遊する片道の時間（秒）",
        CoreEngine::CVarRange{ 0.1f, 10.0f } };

    // ===== monkey.obj の配置・登場演出 =====
    CoreEngine::CVar<float> MonkeyDistance{
        "Title.Monkey.Distance",
        1.0f,
        "トロッコの基準位置から monkey.obj の最終位置までの距離（Y方向）",
        CoreEngine::CVarRange{ 0.0f, 10.0f } };

    CoreEngine::CVar<float> MonkeyIntroDuration{
        "Title.Monkey.IntroDuration",
        0.5f,
        "トロッコ到着後に monkey.obj が飛び出す時間（秒）",
        CoreEngine::CVarRange{ 0.05f, 3.0f } };

    CoreEngine::CVar<float> MonkeyIntroOffset{
        "Title.Monkey.IntroOffset",
        1.5f,
        "monkey.obj をトロッコ内へ沈めておく距離（メートル）",
        CoreEngine::CVarRange{ 0.0f, 10.0f } };

    // ===== 操作ヒント UI =====
    CoreEngine::CVar<float> HintFontSize{
        "Title.UI.HintFontSize",
        28.0f,
        "操作ヒントのフォントサイズ（ピクセル）",
        CoreEngine::CVarRange{ 8.0f, 128.0f } };

    CoreEngine::CVar<CoreEngine::Vector2> HintPosition{
        "Title.UI.HintPosition",
        { 0.0f, -80.0f },
        "画面下中央を基準にした操作ヒントの位置（ピクセル）",
        CoreEngine::CVarRange{ -2000.0f, 2000.0f } };

    CoreEngine::CVar<CoreEngine::Vector4> HintColor{
        "Title.UI.HintColor",
        { 0.85f, 0.92f, 1.0f, 0.95f },
        "操作ヒントの色（RGBA）" };

    CoreEngine::CVar<int> HintSortOrder{
        "Title.UI.HintSortOrder",
        1000,
        "操作ヒントの描画順",
        CoreEngine::CVarRange{ 0.0f, 5000.0f } };

    CoreEngine::CVar<float> HintIntroDelay{
        "Title.UI.HintIntroDelay",
        0.75f,
        "操作ヒントを表示し始めるまでの遅延（秒）",
        CoreEngine::CVarRange{ 0.0f, 5.0f } };

    CoreEngine::CVar<float> HintSlideDistance{
        "Title.UI.HintSlideDistance",
        12.0f,
        "操作ヒントが下からスライドする距離（ピクセル）",
        CoreEngine::CVarRange{ 0.0f, 200.0f } };

    CoreEngine::CVar<float> HintIntroDuration{
        "Title.UI.HintIntroDuration",
        0.35f,
        "操作ヒントのフェード・スライド時間（秒）",
        CoreEngine::CVarRange{ 0.05f, 3.0f } };
}

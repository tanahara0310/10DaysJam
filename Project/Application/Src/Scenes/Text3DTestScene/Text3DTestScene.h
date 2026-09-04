#pragma once

#include "Scene/BaseScene.h"

namespace CoreEngine
{
    class Text3DObject;

    /// @brief 3D テキスト（Text3DObject）の見た目確認用シーン
    /// @details
    ///  1 画面で次を同時に確かめられるように置いてある。
    ///   - 深度: `Test`（遮蔽される）と `Overlay`（壁越しに見える）の対比
    ///   - ビルボード: `None` / `ViewFacing` / `YAxisOnly` を同じ回転量で並べる
    ///   - 組版: 和文の折り返しと禁則、複数行、揃え
    ///   - 距離場の効き: 同じ文字を 4 段階の大きさで並べ、輪郭の鋭さを見る
    ///   - 任意姿勢: 床へ寝かせた文字（ビルボードなしなら回転がそのまま効く）
    ///
    ///  カメラはゆっくり左右に振れる。ビルボードの追従は静止画では判別しにくいので、
    ///  角度が変わったときに「向きっぱなしのもの」と「一緒に回るもの」を見分けるため。
    class Text3DTestScene : public BaseScene {
    public:
        void OnInitialize() override;

    protected:
        void OnUpdate() override;

    private:
        /// @brief 深度モードの対比（球の後ろに文字を置く）
        void BuildDepthCluster();

        /// @brief ビルボード 3 方式の対比（同じ Y 回転を与えて差を見る）
        void BuildBillboardCluster();

        /// @brief 和文の折り返し・禁則・揃え
        void BuildLayoutCluster();

        /// @brief 大きさ違い（距離場なので何倍でも輪郭が鋭い）
        void BuildScaleLadder();

        /// @brief 縁取り・太さのバリエーション
        void BuildStyleRow();

        /// @brief 床へ寝かせた文字（ビルボードなしの任意姿勢）
        void BuildGroundText();

        /// @brief 遮蔽物の球を 1 つ置く
        void CreateOccluderSphere(const Vector3& position, float radius);

        /// @brief クラスタの見出しを 1 つ作る
        Text3DObject* CreateCaption(const std::string& name,
            const std::string& textUtf8, float x);

        /// @brief 3D テキストを 1 つ作る（共通の初期設定をまとめたもの）
        Text3DObject* CreateText(const std::string& name,
            const std::string& textUtf8,
            const Vector3& position,
            float fontSize);
    };
}

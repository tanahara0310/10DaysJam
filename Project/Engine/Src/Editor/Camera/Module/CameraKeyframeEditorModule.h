#pragma once

#ifdef USE_IMGUI

#include "ICameraEditorModule.h"
#include "Camera/Sequence/CameraSequenceEvaluator.h"

#include <string>
#include <vector>

namespace CoreEngine
{
    /// @brief カメラワーク用のキーフレーム編集モジュール
    /// @details 編集中のタイムラインは CameraSequenceAsset そのもの。評価（時刻 → カメラ姿勢）は
    ///          CameraSequenceEvaluator に任せ、ここは編集操作と UI だけを持つ。
    class CameraKeyframeEditorModule final : public ICameraEditorModule {
    public:
        /// @brief タブ名を取得
        const char* GetTabName() const override { return "キーフレーム"; }

        /// @brief 毎フレーム更新（再生・オートキー・可視化）
        void Update(const CameraEditorContext& context) override;

        /// @brief タブ内容を描画
        void Draw(const CameraEditorContext& context) override;

    private:
        /// @brief Undo/Redo で退避する編集状態
        struct EditorState {
            CameraSequenceAsset sequence;
            float playhead = 0.0f;
            int selectedIndex = -1;
            int selectedShotIndex = -1;
            bool isPlaying = false;
            bool loopPlayback = true;
            float playbackSpeed = 1.0f;
        };

        /// @brief アクティブ3Dカメラからスナップショットを取得
        bool CaptureFromActiveCamera(const CameraEditorContext& context, CameraSnapshot& outSnapshot) const;

        /// @brief スナップショットをアクティブ3Dカメラへ適用
        bool ApplyToActiveCamera(const CameraEditorContext& context, const CameraSnapshot& snapshot);

        /// @brief 指定時刻を評価してアクティブ3Dカメラへ反映
        void ApplyEvaluatedAt(const CameraEditorContext& context, float time);

        /// @brief 注視対象をシーンから引く解決口を組み立てる
        CameraSequenceAimContext MakeAimContext(const CameraEditorContext& context) const;

        /// @brief スナップショットが同一かを誤差込みで判定
        bool IsSameSnapshot(const CameraSnapshot& lhs, const CameraSnapshot& rhs) const;

        /// @brief Auto Key更新を実行
        void UpdateAutoKey(const CameraEditorContext& context);

        /// @brief Sceneビュー向けのカメラワーク可視化を描画
        void DrawViewportVisualization(const CameraEditorContext& context);

        /// @brief 指定時刻に最も近いキーフレームを検索
        int FindNearestKeyframeIndex(float time) const;

        /// @brief 指定時刻より前の最も近いキーフレームを検索
        int FindPreviousKeyframeIndex(float time) const;

        /// @brief 指定時刻より後の最も近いキーフレームを検索
        int FindNextKeyframeIndex(float time) const;

        /// @brief シーケンスファイル一覧を更新
        void RefreshClipFileList();

        /// @brief 現在の編集状態をシーケンスとしてファイル保存
        bool SaveCurrentClipToFile(const std::string& filePath) const;

        /// @brief ファイルからシーケンスを読み込み
        bool LoadClipFromFile(const std::string& filePath);

        /// @brief 現在の編集状態を取得
        EditorState CaptureEditorState() const;

        /// @brief 編集状態を適用
        void ApplyEditorState(const EditorState& state);

        /// @brief Undo用に現在状態を保存
        void PushUndoState();

        /// @brief Undoを実行
        void Undo();

        /// @brief Redoを実行
        void Redo();

    private:
        // 編集中のシーケンス本体（キー・ショット・タイムライン長・イージングを含む）
        CameraSequenceAsset sequence_;

        // 編集・再生の状態（シーケンスには保存しない、この画面だけの状態）
        float playhead_ = 0.0f;
        int selectedIndex_ = -1;
        int selectedShotIndex_ = -1;
        float updateThreshold_ = 0.01f;
        bool isPlaying_ = false;
        bool loopPlayback_ = true;
        float playbackSpeed_ = 1.0f;

        int editingShotNameIndex_ = -1;
        char shotNameBuffer_[128] = "";

        // シーケンス保存/読み込み
        char clipFileNameBuffer_[128] = "新規カメラシーケンス";
        std::string clipDirectoryPath_ = CameraSequencePaths::kDirectory;
        std::vector<std::string> clipFileList_;
        int selectedClipFileIndex_ = -1;
        bool needRefreshClipFileList_ = true;

        // Undo/Redo
        std::vector<EditorState> undoStack_;
        std::vector<EditorState> redoStack_;
        size_t maxHistoryCount_ = 64;

        // Auto Key
        bool autoKeyEnabled_ = false;
        bool ignoreNextAutoKey_ = false;
        bool hasObservedSnapshot_ = false;
        bool autoKeyEditing_ = false;
        CameraSnapshot observedSnapshot_{};

        // ビューポート可視化
        bool viewportVisualizationEnabled_ = true;
        bool viewportShowTrajectory_ = true;
        bool viewportShowKeyMarkers_ = true;
        bool viewportShowDebugTarget_ = true;
        int viewportTrajectorySamplesPerSegment_ = 12;
        float viewportMarkerSize_ = 0.2f;
        Vector3 viewportTrajectoryColor_ = { 1.0f, 0.8f, 0.2f };
        Vector3 viewportKeyMarkerColor_ = { 0.2f, 0.8f, 1.0f };
        Vector3 viewportSelectedKeyColor_ = { 1.0f, 0.4f, 0.2f };
        Vector3 viewportDebugTargetColor_ = { 0.2f, 1.0f, 0.3f };
        float viewportTrajectoryAlpha_ = 0.9f;
    };
}

#endif // USE_IMGUI

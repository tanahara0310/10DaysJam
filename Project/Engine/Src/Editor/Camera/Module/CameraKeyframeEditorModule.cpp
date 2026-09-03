#include "pch.h"
#include "CameraKeyframeEditorModule.h"
#include "Camera/Sequence/CameraSequenceEvaluator.h"
#include "Camera/Sequence/CameraSequenceIO.h"

#ifdef USE_IMGUI

#include "Editor/ImGui/ImGuiAll.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

#include "Camera/CameraManager.h"
#include "Camera/Camera.h"
#include "Graphics/Line/LineManager.h"
#include "Utility/JsonManager/JsonManager.h"

namespace CoreEngine
{
    namespace
    {
        // 遷移方式の表示名。CameraSequenceTransitionType の値をそのまま添字に使う。
        constexpr const char* kShotTransitionLabels[] = {
            "カット",
            "ブレンド"
        };

        constexpr int kShotTransitionLabelCount = static_cast<int>(sizeof(kShotTransitionLabels) / sizeof(kShotTransitionLabels[0]));

        // 補間方式の表示名。CameraSequenceInterpolation の値をそのまま添字に使う。
        constexpr const char* kInterpolationLabels[] = {
            "ステップ (補間しない)",
            "直線",
            "スムーズ (Catmull-Rom)"
        };

        constexpr int kInterpolationLabelCount = static_cast<int>(sizeof(kInterpolationLabels) / sizeof(kInterpolationLabels[0]));
    }

    void CameraKeyframeEditorModule::Update(const CameraEditorContext& context)
    {
        if (!context.cameraManager || !isPlaying_ || sequence_.keyframes.size() < 2) {
            UpdateAutoKey(context);
            DrawViewportVisualization();
            return;
        }

        // 再生ヘッドを進め、補間結果を現在カメラへ適用する。
        playhead_ += ImGui::GetIO().DeltaTime * playbackSpeed_;

        if (playhead_ > sequence_.timelineLength) {
            if (loopPlayback_) {
                playhead_ = 0.0f;
            } else {
                playhead_ = sequence_.timelineLength;
                isPlaying_ = false;
            }
        }

        ApplyEvaluatedAt(context, playhead_);

        UpdateAutoKey(context);
        DrawViewportVisualization();
    }

    void CameraKeyframeEditorModule::Draw(const CameraEditorContext& context)
    {
        if (!context.cameraManager) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
            Undo();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
            Redo();
        }

        if (needRefreshClipFileList_) {
            RefreshClipFileList();
        }

        ImGui::Text("アクティブ3D: %s", context.cameraManager->GetActiveCameraName(CameraType::Camera3D).c_str());
        if (ImGui::Button("Undo")) {
            Undo();
        }
        UI::SameLine();
        if (ImGui::Button("Redo")) {
            Redo();
        }
        UI::SameLine();
        UI::Hint("ショートカット: Ctrl+Z / Ctrl+Y");
        UI::Separator();

        UI::Widgets::ToggleSwitch("Auto Key", &autoKeyEnabled_);
        UI::SameLine();
        UI::Hint("Transform/Parametersの変更時に自動でキーを作成・更新");

        UI::SectionHeader("ビューポート可視化");
        UI::Widgets::ToggleSwitch("可視化を有効", &viewportVisualizationEnabled_);
        UI::Widgets::ToggleSwitch("カメラ軌跡", &viewportShowTrajectory_);
        UI::Widgets::ToggleSwitch("キーフレーム位置", &viewportShowKeyMarkers_);
        UI::Widgets::ToggleSwitch("DebugCamera注視点", &viewportShowDebugTarget_);
        UI::DragInt("軌跡サンプル/区間", viewportTrajectorySamplesPerSegment_, 1.0f, 2, 64);
        UI::DragFloat("マーカーサイズ", viewportMarkerSize_, 0.01f, 0.02f, 2.0f, "%.2f");
        UI::SliderFloat("軌跡アルファ", viewportTrajectoryAlpha_, 0.1f, 1.0f, "%.2f");
        UI::ColorEdit3("軌跡色", viewportTrajectoryColor_);
        UI::ColorEdit3("キー色", viewportKeyMarkerColor_);
        UI::ColorEdit3("選択キー色", viewportSelectedKeyColor_);
        UI::ColorEdit3("注視点色", viewportDebugTargetColor_);

        viewportTrajectorySamplesPerSegment_ = std::clamp(viewportTrajectorySamplesPerSegment_, 2, 64);
        viewportMarkerSize_ = std::clamp(viewportMarkerSize_, 0.02f, 2.0f);

        // タイムライン長と再生ヘッドを編集する。
        UI::DragFloat("タイムライン長(秒)", sequence_.timelineLength, 0.1f, 0.1f, 600.0f, "%.2f");
        if (sequence_.timelineLength < 0.1f) {
            sequence_.timelineLength = 0.1f;
        }

        bool playheadChanged = UI::SliderFloat("再生ヘッド", playhead_, 0.0f, sequence_.timelineLength, "%.2f 秒");

        // タイムライン上のキーフレームを直接クリックして移動できるように可視化する。
        {
            const ImVec2 region = ImGui::GetContentRegionAvail();
            const float timelineHeight = 34.0f;
            const ImVec2 cursor = ImGui::GetCursorScreenPos();
            const ImVec2 size((std::max)(region.x, 120.0f), timelineHeight);

            ImGui::InvisibleButton("##KeyframeTimeline", size);
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const ImU32 lineColor = IM_COL32(120, 120, 120, 255);
            const ImU32 keyColor = IM_COL32(80, 170, 255, 255);
            const ImU32 selectedKeyColor = IM_COL32(255, 205, 80, 255);
            const ImU32 playheadColor = IM_COL32(255, 120, 120, 255);

            const float centerY = cursor.y + size.y * 0.5f;
            drawList->AddLine(ImVec2(cursor.x, centerY), ImVec2(cursor.x + size.x, centerY), lineColor, 2.0f);

            // キーフレームマーカー描画
            for (int i = 0; i < static_cast<int>(sequence_.keyframes.size()); ++i) {
                const float normalized = (sequence_.timelineLength > 0.0f) ? (sequence_.keyframes[i].time / sequence_.timelineLength) : 0.0f;
                const float x = cursor.x + (std::clamp(normalized, 0.0f, 1.0f) * size.x);
                const ImU32 color = (i == selectedIndex_) ? selectedKeyColor : keyColor;
                drawList->AddCircleFilled(ImVec2(x, centerY), 4.5f, color);
            }

            // 再生ヘッド描画
            const float playheadX = cursor.x + ((playhead_ / sequence_.timelineLength) * size.x);
            drawList->AddLine(ImVec2(playheadX, cursor.y), ImVec2(playheadX, cursor.y + size.y), playheadColor, 2.0f);

            // クリック時、近いキーフレームがあれば選択、なければ再生ヘッド移動
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const float mouseX = ImGui::GetMousePos().x;
                int nearest = -1;
                float nearestPixelDist = 10.0f;

                for (int i = 0; i < static_cast<int>(sequence_.keyframes.size()); ++i) {
                    const float normalized = (sequence_.timelineLength > 0.0f) ? (sequence_.keyframes[i].time / sequence_.timelineLength) : 0.0f;
                    const float x = cursor.x + (std::clamp(normalized, 0.0f, 1.0f) * size.x);
                    const float d = std::fabs(mouseX - x);
                    if (d < nearestPixelDist) {
                        nearestPixelDist = d;
                        nearest = i;
                    }
                }

                if (nearest >= 0) {
                    selectedIndex_ = nearest;
                    playhead_ = sequence_.keyframes[nearest].time;
                    playheadChanged = true;
                } else {
                    const float normalizedMouse = std::clamp((mouseX - cursor.x) / size.x, 0.0f, 1.0f);
                    playhead_ = normalizedMouse * sequence_.timelineLength;
                    playheadChanged = true;
                }
            }
        }

        if (isPlaying_) {
            if (ImGui::Button("停止")) {
                isPlaying_ = false;
            }
        } else {
            if (ImGui::Button("再生")) {
                isPlaying_ = true;
            }
        }

        UI::SameLine();
        if (ImGui::Button("先頭へ")) {
            playhead_ = 0.0f;
            if (!sequence_.keyframes.empty()) {
                ApplyEvaluatedAt(context, playhead_);
            }
        }

        UI::SameLine();
        UI::Widgets::ToggleSwitch("ループ再生", &loopPlayback_);

        UI::DragFloat("再生速度", playbackSpeed_, 0.05f, 0.1f, 4.0f, "%.2fx");

        if (sequence_.easingTypeIndex < 0 || sequence_.easingTypeIndex >= CameraSequenceEasing::Count()) {
            sequence_.easingTypeIndex = 0;
        }

        if (ImGui::BeginCombo("補間タイプ", CameraSequenceEasing::LabelAt(sequence_.easingTypeIndex))) {
            for (int i = 0; i < CameraSequenceEasing::Count(); ++i) {
                const bool selected = (i == sequence_.easingTypeIndex);
                if (ImGui::Selectable(CameraSequenceEasing::LabelAt(i), selected)) {
                    sequence_.easingTypeIndex = i;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // 前後キーフレームへワンクリック移動できるようにし、編集導線を短くする。
        if (ImGui::Button("前のキーへ")) {
            const int prev = FindPreviousKeyframeIndex(playhead_ - updateThreshold_);
            if (prev >= 0) {
                selectedIndex_ = prev;
                playhead_ = sequence_.keyframes[prev].time;
                playheadChanged = true;
            }
        }

        UI::SameLine();
        if (ImGui::Button("次のキーへ")) {
            const int next = FindNextKeyframeIndex(playhead_ + updateThreshold_);
            if (next >= 0) {
                selectedIndex_ = next;
                playhead_ = sequence_.keyframes[next].time;
                playheadChanged = true;
            }
        }

        // 現在時刻の近傍キーフレームがあれば更新、なければ追加する。
        if (ImGui::Button("現在位置にキーフレームを追加/更新")) {
            CameraSnapshot snapshot;
            if (CaptureFromActiveCamera(context, snapshot)) {
                PushUndoState();
                const int nearest = FindNearestKeyframeIndex(playhead_);
                if (nearest >= 0 && std::fabs(sequence_.keyframes[nearest].time - playhead_) <= updateThreshold_) {
                    sequence_.keyframes[nearest].snapshot = snapshot;
                    selectedIndex_ = nearest;
                } else {
                    CameraSequenceKeyframe key{};
                    key.time = playhead_;
                    key.snapshot = snapshot;
                    sequence_.keyframes.push_back(key);
                    std::sort(sequence_.keyframes.begin(), sequence_.keyframes.end(),
                        [](const CameraSequenceKeyframe& a, const CameraSequenceKeyframe& b) { return a.time < b.time; });

                    selectedIndex_ = FindNearestKeyframeIndex(playhead_);
                }
            }
        }

        UI::SameLine();
        if (ImGui::Button("選択キーフレームを複製")) {
            if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(sequence_.keyframes.size())) {
                PushUndoState();

                CameraSequenceKeyframe duplicated = sequence_.keyframes[selectedIndex_];
                duplicated.time = std::clamp(duplicated.time + 0.1f, 0.0f, sequence_.timelineLength);
                if (std::fabs(duplicated.time - sequence_.keyframes[selectedIndex_].time) <= updateThreshold_) {
                    duplicated.time = std::clamp(duplicated.time + updateThreshold_, 0.0f, sequence_.timelineLength);
                }

                sequence_.keyframes.push_back(duplicated);
                std::sort(sequence_.keyframes.begin(), sequence_.keyframes.end(),
                    [](const CameraSequenceKeyframe& a, const CameraSequenceKeyframe& b) { return a.time < b.time; });

                selectedIndex_ = FindNearestKeyframeIndex(duplicated.time);
                playhead_ = duplicated.time;
                playheadChanged = true;
            }
        }

        UI::SameLine();
        if (ImGui::Button("選択キーフレームを削除")) {
            if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(sequence_.keyframes.size())) {
                PushUndoState();
                sequence_.keyframes.erase(sequence_.keyframes.begin() + selectedIndex_);
                if (sequence_.keyframes.empty()) {
                    selectedIndex_ = -1;
                } else if (selectedIndex_ >= static_cast<int>(sequence_.keyframes.size())) {
                    selectedIndex_ = static_cast<int>(sequence_.keyframes.size()) - 1;
                }
            }
        }

        UI::SameLine();
        if (ImGui::Button("全キーフレームをクリア")) {
            PushUndoState();
            sequence_.keyframes.clear();
            selectedIndex_ = -1;
        }

        UI::Separator();

        if (ImGui::CollapsingHeader("詳細: ショット遷移", ImGuiTreeNodeFlags_None)) {
            UI::Widgets::ToggleSwitch("ショット遷移を有効", &sequence_.shotsEnabled);

            if (ImGui::Button("現在位置にショットを追加")) {
                PushUndoState();

                CameraSequenceShot shot{};
                shot.name = "ショット" + std::to_string(sequence_.shots.size() + 1);
                shot.startTime = std::clamp(playhead_ - 0.5f, 0.0f, sequence_.timelineLength);
                shot.endTime = std::clamp(playhead_ + 0.5f, 0.0f, sequence_.timelineLength);
                if (shot.endTime <= shot.startTime + 0.01f) {
                    shot.endTime = std::clamp(shot.startTime + 0.01f, 0.01f, sequence_.timelineLength);
                }

                sequence_.shots.push_back(shot);
                selectedShotIndex_ = static_cast<int>(sequence_.shots.size()) - 1;
                editingShotNameIndex_ = -1;
            }

            UI::SameLine();
            if (ImGui::Button("現在位置のショットを選択")) {
                selectedShotIndex_ = CameraSequenceEvaluator::FindShotIndexAt(sequence_, playhead_);
                editingShotNameIndex_ = -1;
            }

            UI::SameLine();
            if (ImGui::Button("選択ショットを削除")) {
                if (selectedShotIndex_ >= 0 && selectedShotIndex_ < static_cast<int>(sequence_.shots.size())) {
                    PushUndoState();
                    sequence_.shots.erase(sequence_.shots.begin() + selectedShotIndex_);
                    if (sequence_.shots.empty()) {
                        selectedShotIndex_ = -1;
                    } else {
                        selectedShotIndex_ = std::clamp(selectedShotIndex_, 0, static_cast<int>(sequence_.shots.size()) - 1);
                    }
                    editingShotNameIndex_ = -1;
                }
            }

            if (sequence_.shots.empty()) {
                UI::Hint("ショットがありません。必要な場合のみ追加してください。");
            } else {
                selectedShotIndex_ = std::clamp(selectedShotIndex_, -1, static_cast<int>(sequence_.shots.size()) - 1);

                if (auto lb = UI::Scope::ListBoxScope("ショット一覧", ImVec2(-1.0f, 100.0f))) {
                    for (int i = 0; i < static_cast<int>(sequence_.shots.size()); ++i) {
                        char label[256]{};
                        std::snprintf(label, sizeof(label), "%s [%.2f - %.2f]%s",
                            sequence_.shots[i].name.c_str(),
                            sequence_.shots[i].startTime,
                            sequence_.shots[i].endTime,
                            sequence_.shots[i].enabled ? "" : " (無効)");

                        const bool selected = (selectedShotIndex_ == i);
                        if (ImGui::Selectable(label, selected)) {
                            selectedShotIndex_ = i;
                            editingShotNameIndex_ = -1;
                        }
                    }
                }
            }

            if (selectedShotIndex_ >= 0 && selectedShotIndex_ < static_cast<int>(sequence_.shots.size())) {
                CameraSequenceShot& shot = sequence_.shots[selectedShotIndex_];

                if (editingShotNameIndex_ != selectedShotIndex_) {
                    std::snprintf(shotNameBuffer_, sizeof(shotNameBuffer_), "%s", shot.name.c_str());
                    editingShotNameIndex_ = selectedShotIndex_;
                }

                if (UI::InputText("ショット名", shotNameBuffer_, sizeof(shotNameBuffer_))) {
                    PushUndoState();
                    shot.name = shotNameBuffer_;
                }

                CameraSequenceShot editedShot = shot;
                bool shotChanged = false;

                shotChanged |= UI::Widgets::ToggleSwitch("ショットを有効", &editedShot.enabled);
                shotChanged |= UI::DragFloat("開始時刻", editedShot.startTime, 0.05f, 0.0f, sequence_.timelineLength, "%.2f 秒");
                shotChanged |= UI::DragFloat("終了時刻", editedShot.endTime, 0.05f, 0.0f, sequence_.timelineLength, "%.2f 秒");

                int transitionIndex = static_cast<int>(editedShot.transitionType);
                if (transitionIndex < 0 || transitionIndex >= kShotTransitionLabelCount) {
                    transitionIndex = 0;
                }

                if (ImGui::BeginCombo("遷移", kShotTransitionLabels[transitionIndex])) {
                    for (int i = 0; i < kShotTransitionLabelCount; ++i) {
                        const bool selected = (i == transitionIndex);
                        if (ImGui::Selectable(kShotTransitionLabels[i], selected)) {
                            transitionIndex = i;
                            shotChanged = true;
                        }
                        if (selected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                editedShot.transitionType = static_cast<CameraSequenceTransitionType>(transitionIndex);

                if (editedShot.transitionType == CameraSequenceTransitionType::Blend) {
                    shotChanged |= UI::DragFloat("ブレンド時間", editedShot.blendDuration, 0.01f, 0.0f, sequence_.timelineLength, "%.2f 秒");
                }

                editedShot.startTime = std::clamp(editedShot.startTime, 0.0f, sequence_.timelineLength);
                editedShot.endTime = std::clamp(editedShot.endTime, 0.0f, sequence_.timelineLength);
                if (editedShot.endTime <= editedShot.startTime) {
                    editedShot.endTime = std::clamp(editedShot.startTime + 0.01f, 0.01f, sequence_.timelineLength);
                }

                if (editedShot.blendDuration < 0.0f) {
                    editedShot.blendDuration = 0.0f;
                }

                if (shotChanged) {
                    PushUndoState();
                    shot = editedShot;
                }

                if (ImGui::Button("再生ヘッドをショット先頭へ")) {
                    playhead_ = shot.startTime;
                    playheadChanged = true;
                }
            }
        }

        UI::Separator();

        if (sequence_.keyframes.empty()) {
            UI::Hint("キーフレームがありません。");
            return;
        }

        // キーフレーム一覧表示
        if (auto lb = UI::Scope::ListBoxScope("キーフレーム一覧", ImVec2(-1.0f, 180.0f))) {
            for (int i = 0; i < static_cast<int>(sequence_.keyframes.size()); ++i) {
                const bool isSelected = (i == selectedIndex_);
                char timeLabel[32]{};
                std::snprintf(timeLabel, sizeof(timeLabel), "%.2f秒", sequence_.keyframes[i].time);
                const std::string label = timeLabel;
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    selectedIndex_ = i;
                }
            }
        }

        // 選択したキーフレームを現在のアクティブカメラへ適用
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(sequence_.keyframes.size())) {
            if (ImGui::Button("選択キーフレームを適用")) {
                ApplyToActiveCamera(context, sequence_.keyframes[selectedIndex_].snapshot);
            }

            UI::SameLine();
            if (ImGui::Button("再生ヘッドを選択位置へ移動")) {
                playhead_ = sequence_.keyframes[selectedIndex_].time;
                playheadChanged = true;
            }

            // 選択キーの時刻を直接編集できるようにする。
            float selectedTime = sequence_.keyframes[selectedIndex_].time;
            if (UI::DragFloat("選択キー時刻(秒)", selectedTime, 0.05f, 0.0f, sequence_.timelineLength, "%.2f")) {
                PushUndoState();
                sequence_.keyframes[selectedIndex_].time = std::clamp(selectedTime, 0.0f, sequence_.timelineLength);
                std::sort(sequence_.keyframes.begin(), sequence_.keyframes.end(),
                    [](const CameraSequenceKeyframe& a, const CameraSequenceKeyframe& b) { return a.time < b.time; });
                selectedIndex_ = FindNearestKeyframeIndex(selectedTime);
                playhead_ = std::clamp(selectedTime, 0.0f, sequence_.timelineLength);
                playheadChanged = true;
            }

            // ここから次のキーまでの区間をどう繋ぐか。緩急も補間方式も区間の始点キーが持つ。
            UI::SectionHeader("次のキーへの繋ぎ方");

            CameraSequenceKeyframe& selectedKey = sequence_.keyframes[selectedIndex_];

            int interpolationIndex = static_cast<int>(selectedKey.interpolation);
            if (interpolationIndex < 0 || interpolationIndex >= kInterpolationLabelCount) {
                interpolationIndex = static_cast<int>(CameraSequenceInterpolation::Linear);
            }

            if (ImGui::BeginCombo("補間方式", kInterpolationLabels[interpolationIndex])) {
                for (int i = 0; i < kInterpolationLabelCount; ++i) {
                    const bool selected = (i == interpolationIndex);
                    if (ImGui::Selectable(kInterpolationLabels[i], selected)) {
                        PushUndoState();
                        selectedKey.interpolation = static_cast<CameraSequenceInterpolation>(i);
                        playheadChanged = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            UI::Hint("スムーズは前後のキーも見て曲線で繋ぎます（キーの上は必ず通ります）。");

            // 既定を選べるようにするため、先頭に「シーケンス既定」の項目を足す。
            const bool usesDefault = (selectedKey.easingTypeIndex == kUseSequenceEasing);
            char defaultLabel[128]{};
            std::snprintf(defaultLabel, sizeof(defaultLabel), "シーケンス既定 (%s)",
                CameraSequenceEasing::LabelAt(sequence_.easingTypeIndex));

            const char* currentEasingLabel = usesDefault
                ? defaultLabel
                : CameraSequenceEasing::LabelAt(selectedKey.easingTypeIndex);

            if (ImGui::BeginCombo("この区間の緩急", currentEasingLabel)) {
                if (ImGui::Selectable(defaultLabel, usesDefault)) {
                    PushUndoState();
                    selectedKey.easingTypeIndex = kUseSequenceEasing;
                    playheadChanged = true;
                }
                for (int i = 0; i < CameraSequenceEasing::Count(); ++i) {
                    const bool selected = (!usesDefault && i == selectedKey.easingTypeIndex);
                    if (ImGui::Selectable(CameraSequenceEasing::LabelAt(i), selected)) {
                        PushUndoState();
                        selectedKey.easingTypeIndex = i;
                        playheadChanged = true;
                    }
                    if (selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }

        // 再生停止中に再生ヘッドを動かした場合は、その時刻の値を即時反映する。
        if (!isPlaying_ && playheadChanged) {
            ApplyEvaluatedAt(context, playhead_);
        }

        UI::Separator();
        UI::SectionHeader("シーケンス資産");
        UI::Hint("実ゲームで使うデータは、このシーケンス(.json)です。ショットはシーケンス内の補助情報です。");
        UI::InputText("シーケンス名", clipFileNameBuffer_, sizeof(clipFileNameBuffer_));

        if (ImGui::Button("シーケンスを保存")) {
            std::string fileName = clipFileNameBuffer_;
            if (!fileName.empty()) {
                if (fileName.find(".json") == std::string::npos) {
                    fileName += ".json";
                }

                JsonManager::GetInstance().CreateJsonDirectory(clipDirectoryPath_);
                const std::filesystem::path fullPath = std::filesystem::path(clipDirectoryPath_) / fileName;
                if (SaveCurrentClipToFile(fullPath.string())) {
                    needRefreshClipFileList_ = true;
                }
            }
        }

        UI::SameLine();
        if (ImGui::Button("一覧更新")) {
            needRefreshClipFileList_ = true;
        }

        if (clipFileList_.empty()) {
            UI::Hint("保存済みシーケンスがありません。");
        } else {
            selectedClipFileIndex_ = std::clamp(selectedClipFileIndex_, -1, static_cast<int>(clipFileList_.size()) - 1);

            if (auto lb = UI::Scope::ListBoxScope("シーケンス一覧", ImVec2(-1.0f, 120.0f))) {
                for (int i = 0; i < static_cast<int>(clipFileList_.size()); ++i) {
                    const bool isSelected = (selectedClipFileIndex_ == i);
                    if (ImGui::Selectable(clipFileList_[i].c_str(), isSelected)) {
                        selectedClipFileIndex_ = i;
                    }
                }
            }

            if (selectedClipFileIndex_ >= 0 && selectedClipFileIndex_ < static_cast<int>(clipFileList_.size())) {
                if (ImGui::Button("選択シーケンスを読み込み")) {
                    const std::filesystem::path fullPath = std::filesystem::path(clipDirectoryPath_) / clipFileList_[selectedClipFileIndex_];
                    PushUndoState();
                    if (LoadClipFromFile(fullPath.string())) {
                        // 先頭状態を反映し、読み込み後すぐ内容を確認できるようにする。
                        ApplyEvaluatedAt(context, playhead_);
                    } else {
                        // 読み込み失敗時は不要なUndo履歴を取り消す。
                        if (!undoStack_.empty()) {
                            undoStack_.pop_back();
                        }
                    }
                }
            }
        }
    }

    bool CameraKeyframeEditorModule::CaptureFromActiveCamera(const CameraEditorContext& context, CameraSnapshot& outSnapshot) const
    {
        Camera* active3D = context.cameraManager->GetActiveCamera(CameraType::Camera3D);
        if (!active3D) {
            return false;
        }

        outSnapshot = active3D->CaptureSnapshot("Keyframe");
        return true;
    }

    bool CameraKeyframeEditorModule::ApplyToActiveCamera(const CameraEditorContext& context, const CameraSnapshot& snapshot)
    {
        Camera* active3D = context.cameraManager->GetActiveCamera(CameraType::Camera3D);
        if (!active3D) {
            return false;
        }

        active3D->RestoreSnapshot(snapshot);
        ignoreNextAutoKey_ = true;
        observedSnapshot_ = snapshot;
        hasObservedSnapshot_ = true;
        return true;
    }

    void CameraKeyframeEditorModule::ApplyEvaluatedAt(const CameraEditorContext& context, float time)
    {
        CameraSnapshot evaluated{};
        if (CameraSequenceEvaluator::Evaluate(sequence_, time, evaluated)) {
            ApplyToActiveCamera(context, evaluated);
        }
    }

    bool CameraKeyframeEditorModule::IsSameSnapshot(const CameraSnapshot& lhs, const CameraSnapshot& rhs) const
    {
        constexpr float epsilon = 0.0001f;

        const auto nearEqual = [](float a, float b) {
            return std::fabs(a - b) <= 0.0001f;
        };

        if (lhs.parameters.projectionType != rhs.parameters.projectionType) {
            return false;
        }

        if (!nearEqual(lhs.parameters.fov, rhs.parameters.fov)
            || !nearEqual(lhs.parameters.nearClip, rhs.parameters.nearClip)
            || !nearEqual(lhs.parameters.farClip, rhs.parameters.farClip)
            || !nearEqual(lhs.parameters.aspectRatio, rhs.parameters.aspectRatio)) {
            return false;
        }

        return nearEqual(lhs.position.x, rhs.position.x)
            && nearEqual(lhs.position.y, rhs.position.y)
            && nearEqual(lhs.position.z, rhs.position.z)
            && nearEqual(lhs.rotation.x, rhs.rotation.x)
            && nearEqual(lhs.rotation.y, rhs.rotation.y)
            && nearEqual(lhs.rotation.z, rhs.rotation.z)
            && nearEqual(lhs.scale.x, rhs.scale.x)
            && nearEqual(lhs.scale.y, rhs.scale.y)
            && nearEqual(lhs.scale.z, rhs.scale.z)
            && std::fabs(lhs.parameters.fov - rhs.parameters.fov) <= epsilon;
    }

    void CameraKeyframeEditorModule::UpdateAutoKey(const CameraEditorContext& context)
    {
        // オートキー: カメラを動かした「あと」に自動でキーを打つ。
        // 直前のスナップショットと比べて差が出た時だけ反応させ、
        // 再生中や、こちらがカメラへ書き込んだ直後（ignoreNextAutoKey_）は無視する
        if (!context.cameraManager || !autoKeyEnabled_ || isPlaying_) {
            autoKeyEditing_ = false;
            return;
        }

        CameraSnapshot current{};
        if (!CaptureFromActiveCamera(context, current)) {
            autoKeyEditing_ = false;
            hasObservedSnapshot_ = false;
            return;
        }

        if (ignoreNextAutoKey_) {
            ignoreNextAutoKey_ = false;
            observedSnapshot_ = current;
            hasObservedSnapshot_ = true;
            autoKeyEditing_ = false;
            return;
        }

        if (!hasObservedSnapshot_) {
            observedSnapshot_ = current;
            hasObservedSnapshot_ = true;
            autoKeyEditing_ = false;
            return;
        }

        const bool changed = !IsSameSnapshot(observedSnapshot_, current);
        if (!changed) {
            autoKeyEditing_ = false;
            return;
        }

        if (!autoKeyEditing_) {
            PushUndoState();
            autoKeyEditing_ = true;
        }

        const int nearest = FindNearestKeyframeIndex(playhead_);
        if (nearest >= 0 && std::fabs(sequence_.keyframes[nearest].time - playhead_) <= updateThreshold_) {
            sequence_.keyframes[nearest].snapshot = current;
            selectedIndex_ = nearest;
        } else {
            CameraSequenceKeyframe key{};
            key.time = playhead_;
            key.snapshot = current;
            sequence_.keyframes.push_back(key);
            std::sort(sequence_.keyframes.begin(), sequence_.keyframes.end(),
                [](const CameraSequenceKeyframe& a, const CameraSequenceKeyframe& b) { return a.time < b.time; });

            selectedIndex_ = FindNearestKeyframeIndex(playhead_);
        }

        observedSnapshot_ = current;
    }

    void CameraKeyframeEditorModule::DrawViewportVisualization()
    {
        if (!viewportVisualizationEnabled_ || sequence_.keyframes.empty()) {
            return;
        }

        auto& lineManager = LineManager::GetInstance();

        // スナップショットの position はそのまま視点のワールド座標（軌道パラメータは持たない）。
        if (viewportShowTrajectory_ && sequence_.keyframes.size() >= 2) {
            // 区間ごとの補間結果を細かくサンプルし、Sceneビュー上で軌跡として可視化する。
            for (size_t i = 0; i + 1 < sequence_.keyframes.size(); ++i) {
                const CameraSequenceKeyframe& from = sequence_.keyframes[i];
                const CameraSequenceKeyframe& to = sequence_.keyframes[i + 1];

                if (to.time <= from.time) {
                    continue;
                }

                // 実際の評価をそのままサンプルする。区間ごとの補間方式・緩急が
                // 描かれる軌跡へ反映されるので、見た目と再生結果が食い違わない。
                Vector3 prev = from.snapshot.position;
                for (int sampleIndex = 1; sampleIndex <= viewportTrajectorySamplesPerSegment_; ++sampleIndex) {
                    const float localT = static_cast<float>(sampleIndex) / static_cast<float>(viewportTrajectorySamplesPerSegment_);
                    const float sampleTime = from.time + (to.time - from.time) * localT;

                    CameraSnapshot sampled{};
                    if (!CameraSequenceEvaluator::EvaluateRaw(sequence_, sampleTime, sampled)) {
                        break;
                    }

                    lineManager.DrawLine(prev, sampled.position, viewportTrajectoryColor_, viewportTrajectoryAlpha_);
                    prev = sampled.position;
                }
            }
        }

        if (viewportShowKeyMarkers_) {
            // キーフレーム位置をマーカー表示し、選択中キーを色で区別する。
            for (int i = 0; i < static_cast<int>(sequence_.keyframes.size()); ++i) {
                const Vector3 position = sequence_.keyframes[i].snapshot.position;
                const Vector3 color = (i == selectedIndex_) ? viewportSelectedKeyColor_ : viewportKeyMarkerColor_;
                lineManager.DrawWireSphere(position, viewportMarkerSize_, 8, color, 0.95f);
            }
        }
    }

    int CameraKeyframeEditorModule::FindNearestKeyframeIndex(float time) const
    {
        if (sequence_.keyframes.empty()) {
            return -1;
        }

        int bestIndex = 0;
        float bestDistance = std::fabs(sequence_.keyframes[0].time - time);

        for (int i = 1; i < static_cast<int>(sequence_.keyframes.size()); ++i) {
            const float distance = std::fabs(sequence_.keyframes[i].time - time);
            if (distance < bestDistance) {
                bestDistance = distance;
                bestIndex = i;
            }
        }

        return bestIndex;
    }

    int CameraKeyframeEditorModule::FindPreviousKeyframeIndex(float time) const
    {
        int index = -1;
        for (int i = 0; i < static_cast<int>(sequence_.keyframes.size()); ++i) {
            if (sequence_.keyframes[i].time <= time) {
                index = i;
            } else {
                break;
            }
        }
        return index;
    }

    int CameraKeyframeEditorModule::FindNextKeyframeIndex(float time) const
    {
        for (int i = 0; i < static_cast<int>(sequence_.keyframes.size()); ++i) {
            if (sequence_.keyframes[i].time >= time) {
                return i;
            }
        }
        return -1;
    }

    void CameraKeyframeEditorModule::RefreshClipFileList()
    {
        clipFileList_ = CameraSequenceIO::GetSequenceFileList(clipDirectoryPath_);
        needRefreshClipFileList_ = false;
    }

    bool CameraKeyframeEditorModule::SaveCurrentClipToFile(const std::string& filePath) const
    {
        // 編集中のタイムラインがシーケンスそのものなので、そのまま渡すだけでよい。
        return CameraSequenceIO::Save(filePath, sequence_);
    }

    bool CameraKeyframeEditorModule::LoadClipFromFile(const std::string& filePath)
    {
        // 読み込み成功時だけ内部状態を差し替える。失敗しても編集中のタイムラインは壊さない
        CameraSequenceAsset asset{};
        if (!CameraSequenceIO::Load(filePath, asset)) {
            return false;
        }

        // 時刻の並び・範囲は CameraSequenceIO::Load が整えて返す。
        sequence_ = std::move(asset);

        selectedIndex_ = sequence_.keyframes.empty() ? -1 : 0;
        selectedShotIndex_ = sequence_.shots.empty() ? -1 : 0;
        editingShotNameIndex_ = -1;
        playhead_ = 0.0f;
        isPlaying_ = false;
        return true;
    }

    CameraKeyframeEditorModule::EditorState CameraKeyframeEditorModule::CaptureEditorState() const
    {
        EditorState state{};
        state.sequence = sequence_;
        state.playhead = playhead_;
        state.selectedIndex = selectedIndex_;
        state.selectedShotIndex = selectedShotIndex_;
        state.isPlaying = isPlaying_;
        state.loopPlayback = loopPlayback_;
        state.playbackSpeed = playbackSpeed_;
        return state;
    }

    void CameraKeyframeEditorModule::ApplyEditorState(const EditorState& state)
    {
        sequence_ = state.sequence;
        playhead_ = state.playhead;
        selectedIndex_ = state.selectedIndex;
        selectedShotIndex_ = state.selectedShotIndex;
        isPlaying_ = state.isPlaying;
        loopPlayback_ = state.loopPlayback;
        playbackSpeed_ = state.playbackSpeed;
        editingShotNameIndex_ = -1;
    }

    void CameraKeyframeEditorModule::PushUndoState()
    {
        undoStack_.push_back(CaptureEditorState());
        if (undoStack_.size() > maxHistoryCount_) {
            undoStack_.erase(undoStack_.begin());
        }
        redoStack_.clear();
    }

    void CameraKeyframeEditorModule::Undo()
    {
        if (undoStack_.empty()) {
            return;
        }

        redoStack_.push_back(CaptureEditorState());
        ApplyEditorState(undoStack_.back());
        undoStack_.pop_back();
    }

    void CameraKeyframeEditorModule::Redo()
    {
        if (redoStack_.empty()) {
            return;
        }

        undoStack_.push_back(CaptureEditorState());
        ApplyEditorState(redoStack_.back());
        redoStack_.pop_back();
    }
}

#endif // _DEBUG

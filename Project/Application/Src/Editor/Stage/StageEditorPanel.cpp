#include "pch.h"
#include "StageEditorPanel.h"

#ifdef USE_IMGUI

#include "StageChipPalette.h"

#include "Components/Building/MapGeneratorComponent.h"

#include "Editor/ImGui/ImGuiAll.h"
#include "GameObject/GameObjectManager.h"
#include "Scene/SceneManager.h"
#include "Utility/Debug/GameDebugUI.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <string>

using namespace CoreEngine;

namespace GameEditors
{
    namespace
    {
        /// @brief ファイル一覧の先頭に置く固定CSVの見出し
        constexpr const char* kFixedEntryLabel = "（固定CSV）";

        /// @brief GameScene が列車とビルダーを置くX座標
        /// @note GameScene::OnInitialize の initialBuilderPosX と同じ値。
        ///       Z は mapSizeZ / 2 なので構成から計算する。
        constexpr std::size_t kStartPositionX = 3;

        /// @brief 番号帯の幅と高さ
        constexpr float kRulerWidth = 28.0f;
        constexpr float kRulerHeight = 18.0f;

        /// @brief パレットの色をそのまま ImU32 として使う（並びは IM_COL32 と同じ）
        ImU32 ToImColor(uint32_t packed)
        {
            return static_cast<ImU32>(packed);
        }

        /// @brief マスの色の明るさから、上に載せる文字色を決める
        ImU32 PickTextColor(uint32_t packed)
        {
            const uint32_t r = packed & 0xFFu;
            const uint32_t g = (packed >> 8) & 0xFFu;
            const uint32_t b = (packed >> 16) & 0xFFu;
            const uint32_t luminance = (r * 299u + g * 587u + b * 114u) / 1000u;
            return luminance > 140u ? IM_COL32(24, 24, 24, 255) : IM_COL32(240, 240, 240, 255);
        }

        /// @brief パスからファイル名だけを取り出す
        /// @details std::filesystem::path を経由すると、UTF-8のまま持っている文字列が
        ///          Windowsの既定コードページ扱いで壊れる。区切り文字を自分で探す。
        std::string FileNameOf(const std::string& path)
        {
            const auto position = path.find_last_of("/\\");
            return position == std::string::npos ? path : path.substr(position + 1);
        }

        /// @brief エディタの実体
        /// @details タブを閉じても編集中の内容を残したいので、1つだけ持ち続ける。
        StageEditorPanel& GetPanelInstance()
        {
            static StageEditorPanel instance;
            return instance;
        }

        /// @brief 明るめの色を作る（ホバー表示用）
        ImVec4 Brighten(const ImVec4& color, float amount)
        {
            return ImVec4(
                std::min(color.x + amount, 1.0f),
                std::min(color.y + amount, 1.0f),
                std::min(color.z + amount, 1.0f),
                color.w);
        }
    }

    void StageEditorPanel::Register(GameDebugUI* debugUI, SceneManager* sceneManager)
    {
        if (!debugUI) {
            return;
        }
        GetPanelInstance().Initialize(sceneManager);
        debugUI->RegisterAppEditor("Stage", []() { GetPanelInstance().Draw(); });
    }

    void StageEditorPanel::Initialize(SceneManager* sceneManager)
    {
        sceneManager_ = sceneManager;
        if (initialized_) {
            return;
        }
        initialized_ = true;
        ReloadProject();
        NewDocument(project_.chunkSizeX, project_.mapSizeZ);
        SetStatus("ステージエディタを開きました");
    }

    // ──────────────────────────────────────────────────────────
    // ファイル操作
    // ──────────────────────────────────────────────────────────

    void StageEditorPanel::ReloadProject()
    {
        if (StageProjectIO::Load(projectPath_, project_)) {
            SetStatus("ステージ構成を読み込みました");
        } else {
            // 構成ファイルがまだ無い初回は、エリアフォルダーの中身から組み立てる。
            project_ = StageProjectIO::ScanFromDisk(StageProjectIO::kAreasRoot);
            SetStatus("構成ファイルが無いので、エリアフォルダーから作りました");
        }
        project_.chunkSizeX = std::max<std::size_t>(1, project_.chunkSizeX);
        project_.mapSizeZ = std::max<std::size_t>(1, project_.mapSizeZ);
        newSizeX_ = static_cast<int>(project_.chunkSizeX);
        newSizeZ_ = static_cast<int>(project_.mapSizeZ);
        selectedAreaIndex_ = project_.areas.empty() ? -1 : 0;
        RebuildBrowseList();
        // 現行の生成方式はランダム区画なので、最初から見えるのはエリアの側にしておく。
        selectedBrowseIndex_ = browseList_.size() > 1 ? 1 : 0;
    }

    void StageEditorPanel::RebuildBrowseList()
    {
        browseList_.clear();

        StageAreaDefinition fixedEntry;
        fixedEntry.name = kFixedEntryLabel;
        if (!project_.fixedCsvPath.empty()) {
            fixedEntry.paths.push_back(project_.fixedCsvPath);
        }
        browseList_.push_back(std::move(fixedEntry));

        for (const auto& area : project_.areas) {
            browseList_.push_back(area);
        }

        selectedBrowseIndex_ = std::clamp(selectedBrowseIndex_, 0,
            static_cast<int>(browseList_.size()) - 1);
        selectedCsvIndex_ = -1;
    }

    void StageEditorPanel::OpenCsv(const std::string& path)
    {
        if (!document_.Load(path)) {
            SetStatus("読み込めません: " + path, true);
            return;
        }
        std::snprintf(saveAsBuffer_, sizeof(saveAsBuffer_), "%s", path.c_str());

        std::string message = "読み込みました: " + path;
        const std::size_t invalidCount = document_.GetInvalidCellCount();
        if (invalidCount > 0) {
            message += "（不明なセル " + std::to_string(invalidCount) + " 個は空白として読みました）";
        }
        SetStatus(message, invalidCount > 0);
    }

    void StageEditorPanel::SaveCsv(const std::string& path)
    {
        if (path.empty()) {
            SetStatus("保存先のパスが空です", true);
            return;
        }
        if (!document_.Save(path)) {
            SetStatus("保存できません: " + path, true);
            return;
        }
        std::snprintf(saveAsBuffer_, sizeof(saveAsBuffer_), "%s", path.c_str());
        SetStatus("保存しました: " + path);
    }

    void StageEditorPanel::NewDocument(std::size_t sizeX, std::size_t sizeZ)
    {
        document_.Reset(std::max<std::size_t>(1, sizeX), std::max<std::size_t>(1, sizeZ),
            GameComponents::MapChipType::Ground);
        saveAsBuffer_[0] = '\0';
        SetStatus("新しい区画を作りました。「名前を付けて保存」で保存先を決めてください");
    }

    // ──────────────────────────────────────────────────────────
    // 実行中マップ
    // ──────────────────────────────────────────────────────────

    GameComponents::MapGeneratorComponent* StageEditorPanel::FindMapGenerator() const
    {
        if (!sceneManager_) {
            return nullptr;
        }
        GameObjectManager* objectManager = sceneManager_->GetCurrentGameObjectManager();
        if (!objectManager) {
            return nullptr;
        }
        return objectManager->FindFirstComponent<GameComponents::MapGeneratorComponent>();
    }

    std::size_t StageEditorPanel::GetMaxApplyChunkIndex(
        GameComponents::MapGeneratorComponent* generator) const
    {
        const std::size_t chunkSize = std::max<std::size_t>(1, project_.chunkSizeX);
        const std::size_t generated = generator ? generator->GetMapChips().size() : 0;
        // 遠い区画を指定すると、そこへ届くまでの地形を SetMapChip が一気に作ってしまう。
        // 生成済みの先端から4区画先までに抑える。
        return generated / chunkSize + 4;
    }

    std::size_t StageEditorPanel::GetChunkStartX() const
    {
        const std::size_t chunkSize = std::max<std::size_t>(1, project_.chunkSizeX);
        return static_cast<std::size_t>(std::max(0, applyChunkIndex_)) * chunkSize;
    }

    void StageEditorPanel::ApplyToRuntime(
        GameComponents::MapGeneratorComponent* generator, std::size_t startX)
    {
        if (!generator) {
            return;
        }
        const std::size_t sizeX = document_.GetSizeX();
        const std::size_t sizeZ = document_.GetSizeZ();
        std::size_t skipped = 0;
        for (std::size_t x = 0; x < sizeX; ++x) {
            for (std::size_t z = 0; z < sizeZ; ++z) {
                if (!generator->SetMapChip(startX + x, z, document_.Get(x, z))) {
                    ++skipped;
                }
            }
        }

        std::string message = "X=" + std::to_string(startX) + " から実行中マップへ反映しました";
        if (skipped > 0) {
            message += "（Z方向がマップより広く、" + std::to_string(skipped) + " マスは入りませんでした）";
        }
        SetStatus(message, skipped > 0);
    }

    void StageEditorPanel::CaptureFromRuntime(
        GameComponents::MapGeneratorComponent* generator, std::size_t startX)
    {
        if (!generator) {
            return;
        }
        const std::size_t sizeX = std::max<std::size_t>(1, project_.chunkSizeX);
        // まだ生成していない範囲を求められても、読む前に作らせておく。
        generator->CreateToX(startX + sizeX);

        const auto& mapChips = generator->GetMapChips();
        const std::size_t sizeZ = mapChips.empty()
            ? std::max<std::size_t>(1, project_.mapSizeZ) : mapChips.front().size();

        StageCsvDocument::Grid grid(sizeZ, StageCsvDocument::Row(sizeX,
            GameComponents::MapChipType::Void));
        for (std::size_t x = 0; x < sizeX; ++x) {
            for (std::size_t z = 0; z < sizeZ; ++z) {
                grid[z][x] = generator->GetMapChip(startX + x, z);
            }
        }
        document_.SetGrid(grid);
        SetStatus("X=" + std::to_string(startX) + " からの区画を実行中マップから取り込みました");
    }

    void StageEditorPanel::SetStatus(const std::string& message, bool isError)
    {
        statusMessage_ = message;
        statusIsError_ = isError;
    }

    // ──────────────────────────────────────────────────────────
    // 描画
    // ──────────────────────────────────────────────────────────

    void StageEditorPanel::Draw()
    {
        DrawToolbar();
        UI::Separator();

        if (ImGui::BeginTabBar("##StageEditorTabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (ImGui::BeginTabItem("区画を編集")) {
                DrawEditTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("実行中へ反映")) {
                DrawRuntimeTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("エリア構成")) {
                DrawAreaTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    void StageEditorPanel::DrawToolbar()
    {
        ImGui::TextUnformatted("編集中:");
        UI::SameLine();
        const std::string& path = document_.GetPath();
        if (path.empty()) {
            ImGui::TextDisabled("(新規)");
        } else {
            ImGui::TextColored(ImVec4(0.26f, 0.72f, 0.98f, 1.0f), "%s", FileNameOf(path).c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", path.c_str());
            }
        }
        if (document_.IsDirty()) {
            UI::SameLine();
            ImGui::TextColored(ImVec4(0.98f, 0.72f, 0.26f, 1.0f), "* 未保存");
        }
        UI::SameLine();
        ImGui::TextDisabled("| %zu x %zu マス", document_.GetSizeX(), document_.GetSizeZ());

        if (!statusMessage_.empty()) {
            const ImVec4 color = statusIsError_
                ? ImVec4(0.95f, 0.45f, 0.40f, 1.0f)
                : ImVec4(0.55f, 0.85f, 0.60f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", statusMessage_.c_str());
            ImGui::PopStyleColor();
        }
    }

    void StageEditorPanel::DrawEditTab()
    {
        DrawFileBrowser();
        UI::Separator();
        DrawPalette();
        UI::Separator();
        DrawGridCanvas();
    }

    void StageEditorPanel::DrawFileBrowser()
    {
        UI::SectionHeader("ファイル");

        if (browseList_.empty()) {
            RebuildBrowseList();
        }
        selectedBrowseIndex_ = std::clamp(selectedBrowseIndex_, 0,
            static_cast<int>(browseList_.size()) - 1);

        if (ImGui::BeginCombo("エリア", browseList_[selectedBrowseIndex_].name.c_str())) {
            for (int i = 0; i < static_cast<int>(browseList_.size()); ++i) {
                const bool selected = (i == selectedBrowseIndex_);
                if (ImGui::Selectable(browseList_[i].name.c_str(), selected) && !selected) {
                    selectedBrowseIndex_ = i;
                    selectedCsvIndex_ = -1;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        const std::vector<std::string>& paths = browseList_[selectedBrowseIndex_].paths;
        if (paths.empty()) {
            UI::Hint("このエリアに区画CSVがありません。「エリア構成」タブで追加できます。");
        } else {
            const float listHeight = ImGui::GetTextLineHeightWithSpacing() * 5.0f;
            if (ImGui::BeginListBox("##StageCsvList", ImVec2(-FLT_MIN, listHeight))) {
                for (int i = 0; i < static_cast<int>(paths.size()); ++i) {
                    const bool selected = (i == selectedCsvIndex_);
                    if (ImGui::Selectable(FileNameOf(paths[i]).c_str(), selected)) {
                        selectedCsvIndex_ = i;
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("%s", paths[i].c_str());
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            selectedCsvIndex_ = i;
                            OpenCsv(paths[i]);
                        }
                    }
                }
                ImGui::EndListBox();
            }
        }

        const bool hasSelection = selectedCsvIndex_ >= 0
            && selectedCsvIndex_ < static_cast<int>(paths.size());
        ImGui::BeginDisabled(!hasSelection);
        if (ImGui::Button("開く")) {
            OpenCsv(paths[selectedCsvIndex_]);
        }
        ImGui::EndDisabled();
        UI::SameLine();
        UI::Hint("一覧をダブルクリックしても開けます");

        UI::SectionHeader("新規・保存");

        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputInt("幅X", &newSizeX_);
        UI::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputInt("高さZ", &newSizeZ_);
        newSizeX_ = std::clamp(newSizeX_, 1, 512);
        newSizeZ_ = std::clamp(newSizeZ_, 1, 128);

        if (ImGui::Button("新規")) {
            NewDocument(static_cast<std::size_t>(newSizeX_), static_cast<std::size_t>(newSizeZ_));
        }
        UI::SameLine();
        if (ImGui::Button("このサイズへ変更")) {
            document_.Resize(static_cast<std::size_t>(newSizeX_), static_cast<std::size_t>(newSizeZ_));
            SetStatus("マス数を変えました（増えた分は空白です）");
        }
        UI::SameLine();
        UI::HelpMarker("ゲームは区画を 区画の幅X × マップの高さZ で読みます。"
            "小さいCSVは空白で埋められ、大きいCSVははみ出した分が捨てられます。");

        UI::InputText("保存先", saveAsBuffer_, sizeof(saveAsBuffer_));
        if (ImGui::Button("保存")) {
            SaveCsv(document_.GetPath().empty() ? std::string(saveAsBuffer_) : document_.GetPath());
        }
        UI::SameLine();
        if (ImGui::Button("名前を付けて保存")) {
            SaveCsv(saveAsBuffer_);
        }
        UI::SameLine();
        bool writeVoidAsEmpty = document_.GetWriteVoidAsEmpty();
        if (ImGui::Checkbox("空白を空欄で書く", &writeVoidAsEmpty)) {
            document_.SetWriteVoidAsEmpty(writeVoidAsEmpty);
        }
        UI::SameLine();
        UI::HelpMarker("既存のCSVには空欄とゼロが混ざっています。どちらも空白として読まれます。");
    }

    void StageEditorPanel::DrawPalette()
    {
        UI::SectionHeader("パレット");

        for (std::size_t i = 0; i < kStageChipCount; ++i) {
            const StageChipInfo& info = kStageChipPalette[i];
            if (i > 0) {
                UI::SameLine();
            }
            ImGui::PushID(static_cast<int>(i));

            const ImVec4 base = ImColor(ToImColor(info.color)).Value;
            ImGui::PushStyleColor(ImGuiCol_Button, base);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(base, 0.15f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Brighten(base, 0.25f));

            char label[8] = {};
            std::snprintf(label, sizeof(label), "%d", info.csvId);
            if (ImGui::Button(label, ImVec2(36.0f, 28.0f))) {
                brush_ = info.type;
            }
            ImGui::PopStyleColor(3);

            if (info.type == brush_) {
                ImGui::GetWindowDrawList()->AddRect(
                    ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                    IM_COL32(255, 255, 255, 230), 2.0f, 0, 2.0f);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%s\nCSVの数字: %d / 数字キー %d",
                    info.label, info.hint, info.csvId, info.csvId);
            }
            ImGui::PopID();
        }

        ImGui::Text("選択中: %s", GetChipInfo(brush_).label);
        UI::SameLine();
        UI::HelpMarker("左ドラッグで選択中のチップを塗り、右ドラッグで空白に戻します。"
            "ホイールクリックでその場のチップを吸い取ります。"
            "マップの上では数字キー 0〜4 でも切り替えられます。");
    }

    void StageEditorPanel::DrawGridCanvas()
    {
        UI::SectionHeader("マップ");

        ImGui::SetNextItemWidth(150.0f);
        UI::SliderFloat("マスの大きさ", cellSize_, 12.0f, 48.0f, "%.0f px");
        UI::SameLine();
        ImGui::Checkbox("開始位置", &showStartMarker_);
        UI::SameLine();
        UI::HelpMarker("GameScene が列車とビルダーを置くマス。X=0 の区画に置いたときだけ意味を持ちます。");
        UI::SameLine();
        ImGui::BeginDisabled(!document_.CanUndo());
        if (ImGui::Button("元に戻す")) {
            document_.Undo();
        }
        ImGui::EndDisabled();
        UI::SameLine();
        if (ImGui::Button("全部塗る")) {
            document_.Fill(brush_);
            SetStatus("全マスを塗りました");
        }

        const std::size_t sizeX = document_.GetSizeX();
        const std::size_t sizeZ = document_.GetSizeZ();
        if (sizeX == 0 || sizeZ == 0) {
            UI::Hint("マスがありません。「新規」でサイズを決めてください。");
            return;
        }

        const float cell = cellSize_;
        const float viewHeight = std::min(kRulerHeight + cell * static_cast<float>(sizeZ) + 20.0f, 470.0f);
        ImGui::BeginChild("##StageGridView", ImVec2(0.0f, viewHeight),
            ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 gridOrigin(origin.x + kRulerWidth, origin.y + kRulerHeight);
        const ImVec2 canvasSize(
            kRulerWidth + cell * static_cast<float>(sizeX),
            kRulerHeight + cell * static_cast<float>(sizeZ));

        ImGui::InvisibleButton("##StageGrid", canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
            | ImGuiButtonFlags_MouseButtonMiddle);
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        ImDrawList* draw = ImGui::GetWindowDrawList();

        // ── マス ──
        for (std::size_t z = 0; z < sizeZ; ++z) {
            for (std::size_t x = 0; x < sizeX; ++x) {
                const StageChipInfo& info = GetChipInfo(document_.Get(x, z));
                const ImVec2 cellMin(
                    gridOrigin.x + cell * static_cast<float>(x),
                    gridOrigin.y + cell * static_cast<float>(z));
                const ImVec2 cellMax(cellMin.x + cell, cellMin.y + cell);
                draw->AddRectFilled(cellMin, cellMax, ToImColor(info.color));

                if (info.mark[0] != '\0' && cell >= 18.0f) {
                    const ImVec2 textSize = ImGui::CalcTextSize(info.mark);
                    draw->AddText(
                        ImVec2(cellMin.x + (cell - textSize.x) * 0.5f,
                            cellMin.y + (cell - textSize.y) * 0.5f),
                        PickTextColor(info.color), info.mark);
                }
            }
        }

        // ── 罫線 ──
        const ImU32 lineColor = IM_COL32(0, 0, 0, 90);
        for (std::size_t x = 0; x <= sizeX; ++x) {
            const float lineX = gridOrigin.x + cell * static_cast<float>(x);
            draw->AddLine(ImVec2(lineX, gridOrigin.y),
                ImVec2(lineX, gridOrigin.y + cell * static_cast<float>(sizeZ)), lineColor);
        }
        for (std::size_t z = 0; z <= sizeZ; ++z) {
            const float lineY = gridOrigin.y + cell * static_cast<float>(z);
            draw->AddLine(ImVec2(gridOrigin.x, lineY),
                ImVec2(gridOrigin.x + cell * static_cast<float>(sizeX), lineY), lineColor);
        }

        // ── 番号（左がZ・上がX） ──
        if (cell >= 16.0f) {
            const ImU32 rulerColor = IM_COL32(170, 170, 175, 255);
            char label[16] = {};
            for (std::size_t x = 0; x < sizeX; ++x) {
                std::snprintf(label, sizeof(label), "%zu", x);
                const ImVec2 textSize = ImGui::CalcTextSize(label);
                draw->AddText(ImVec2(
                    gridOrigin.x + cell * static_cast<float>(x) + (cell - textSize.x) * 0.5f,
                    origin.y + 1.0f), rulerColor, label);
            }
            for (std::size_t z = 0; z < sizeZ; ++z) {
                std::snprintf(label, sizeof(label), "%zu", z);
                const ImVec2 textSize = ImGui::CalcTextSize(label);
                draw->AddText(ImVec2(
                    origin.x + kRulerWidth - textSize.x - 4.0f,
                    gridOrigin.y + cell * static_cast<float>(z) + (cell - textSize.y) * 0.5f),
                    rulerColor, label);
            }
        }

        // ── 開始位置 ──
        const std::size_t startZ = std::max<std::size_t>(1, project_.mapSizeZ) / 2;
        if (showStartMarker_ && kStartPositionX < sizeX && startZ < sizeZ) {
            const ImVec2 center(
                gridOrigin.x + cell * (static_cast<float>(kStartPositionX) + 0.5f),
                gridOrigin.y + cell * (static_cast<float>(startZ) + 0.5f));
            draw->AddCircle(center, cell * 0.34f, IM_COL32(255, 255, 255, 230), 0, 2.0f);
            draw->AddCircleFilled(center, cell * 0.12f, IM_COL32(255, 255, 255, 230));
        }

        // ── 入力 ──
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        int hoverX = -1;
        int hoverZ = -1;
        const float localX = (mouse.x - gridOrigin.x) / cell;
        const float localZ = (mouse.y - gridOrigin.y) / cell;
        if (localX >= 0.0f && localZ >= 0.0f
            && localX < static_cast<float>(sizeX) && localZ < static_cast<float>(sizeZ)) {
            hoverX = static_cast<int>(localX);
            hoverZ = static_cast<int>(localZ);
        }

        const bool leftDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

        if ((hovered || active) && hoverX >= 0) {
            const ImVec2 cellMin(
                gridOrigin.x + cell * static_cast<float>(hoverX),
                gridOrigin.y + cell * static_cast<float>(hoverZ));
            draw->AddRect(cellMin, ImVec2(cellMin.x + cell, cellMin.y + cell),
                IM_COL32(255, 255, 255, 200), 0.0f, 0, 2.0f);

            if (leftDown || rightDown) {
                // ドラッグ1回で履歴1件。途中の1マスごとには積まない。
                if (!strokeActive_) {
                    document_.BeginStroke();
                    strokeActive_ = true;
                }
                const GameComponents::MapChipType type = leftDown
                    ? brush_ : GameComponents::MapChipType::Void;
                const std::size_t cellX = static_cast<std::size_t>(hoverX);
                const std::size_t cellZ = static_cast<std::size_t>(hoverZ);
                if (document_.Set(cellX, cellZ, type) && liveApply_) {
                    if (auto* generator = FindMapGenerator()) {
                        generator->SetMapChip(GetChunkStartX() + cellX, cellZ, type);
                    }
                }
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                brush_ = document_.Get(
                    static_cast<std::size_t>(hoverX), static_cast<std::size_t>(hoverZ));
            }
        }
        if (!leftDown && !rightDown) {
            strokeActive_ = false;
        }

        if (hovered) {
            for (std::size_t i = 0; i < kStageChipCount; ++i) {
                const ImGuiKey key = static_cast<ImGuiKey>(
                    ImGuiKey_0 + kStageChipPalette[i].csvId);
                if (ImGui::IsKeyPressed(key, false)) {
                    brush_ = kStageChipPalette[i].type;
                }
            }
            // Ctrl+Z はシーン編集側も使うので、マップの上にいるときだけ拾う。
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
                document_.Undo();
            }
        }

        ImGui::EndChild();

        if (hoverX >= 0) {
            ImGui::Text("カーソル: X=%d, Z=%d  (%s)", hoverX, hoverZ,
                GetChipInfo(document_.Get(static_cast<std::size_t>(hoverX),
                    static_cast<std::size_t>(hoverZ))).label);
        } else {
            UI::Hint("カーソル: -");
        }
    }

    void StageEditorPanel::DrawRuntimeTab()
    {
        UI::SectionHeader("実行中のマップ");

        GameComponents::MapGeneratorComponent* generator = FindMapGenerator();
        if (!generator) {
            UI::Hint("MapGenerator を持つシーンが動いていません。GameScene を開くと使えます。");
            return;
        }

        const auto& mapChips = generator->GetMapChips();
        const std::size_t generated = mapChips.size();
        const std::size_t runtimeSizeZ = mapChips.empty() ? 0 : mapChips.front().size();
        ImGui::Text("生成済み: X = %zu 列 / Z = %zu 行", generated, runtimeSizeZ);

        const std::string activePool = generator->GetActiveCsvPoolName();
        const std::string selectedPool = generator->GetSelectedCsvPoolName();
        ImGui::Text("エリアプール: 生成中 %s / 次の区画 %s",
            activePool.empty() ? "(なし)" : activePool.c_str(),
            selectedPool.empty() ? "(なし)" : selectedPool.c_str());

        UI::SectionHeader("適用先");

        const std::size_t maxChunk = GetMaxApplyChunkIndex(generator);
        ImGui::SetNextItemWidth(150.0f);
        ImGui::InputInt("区画番号", &applyChunkIndex_);
        applyChunkIndex_ = std::clamp(applyChunkIndex_, 0, static_cast<int>(maxChunk));
        UI::SameLine();
        if (ImGui::Button("生成済みの先端へ")) {
            const std::size_t chunkSize = std::max<std::size_t>(1, project_.chunkSizeX);
            applyChunkIndex_ = generated >= chunkSize
                ? static_cast<int>(generated / chunkSize) - 1 : 0;
        }
        UI::SameLine();
        UI::HelpMarker("マップはカメラの少し先まで作られています。"
            "先端の1つ手前の区画へ書けば、走っている列車のすぐ前に出ます。");

        const std::size_t startX = GetChunkStartX();
        const std::size_t sizeX = document_.GetSizeX();
        if (sizeX > 0) {
            ImGui::Text("書き込むX範囲: %zu 〜 %zu", startX, startX + sizeX - 1);
        }
        ImGui::TextDisabled("指定できるのは 0 〜 %zu 区画です", maxChunk);
        UI::SameLine();
        UI::HelpMarker("遠すぎる区画を指定すると、そこへ届くまでの地形がまとめて生成されてしまいます。");

        if (ImGui::Button("実行中マップへ適用")) {
            ApplyToRuntime(generator, startX);
        }
        UI::SameLine();
        if (ImGui::Button("実行中マップから読み込む")) {
            CaptureFromRuntime(generator, startX);
        }

        bool live = liveApply_;
        if (ImGui::Checkbox("塗るたびに反映する", &live)) {
            liveApply_ = live;
            if (liveApply_) {
                ApplyToRuntime(generator, startX);
            }
        }
        UI::SameLine();
        UI::HelpMarker("1マス塗るたびに、同じ位置を実行中のマップへ書き込みます。");

        UI::Separator();
        ImGui::TextWrapped(
            "ここでの反映は実行中のマップだけを書き換えます。CSVは別に保存してください。"
            "次のプレイでは保存したCSVが読まれます。"
            "資源チップは列車が取ると地面へ変わるので、読み込み結果が編集内容と違うことがあります。");
    }

    void StageEditorPanel::DrawAreaTab()
    {
        UI::SectionHeader("ステージ構成");
        ImGui::TextDisabled("%s", projectPath_.c_str());

        int chunkSizeX = static_cast<int>(project_.chunkSizeX);
        int mapSizeZ = static_cast<int>(project_.mapSizeZ);
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputInt("区画の幅X", &chunkSizeX)) {
            project_.chunkSizeX = static_cast<std::size_t>(std::clamp(chunkSizeX, 1, 512));
        }
        UI::SameLine();
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputInt("マップの高さZ", &mapSizeZ)) {
            project_.mapSizeZ = static_cast<std::size_t>(std::clamp(mapSizeZ, 1, 128));
        }

        if (ImGui::BeginCombo("開始エリア",
            project_.initialAreaName.empty() ? "(なし)" : project_.initialAreaName.c_str())) {
            for (const auto& area : project_.areas) {
                const bool selected = (area.name == project_.initialAreaName);
                if (ImGui::Selectable(area.name.c_str(), selected)) {
                    project_.initialAreaName = area.name;
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        char fixedBuffer[260] = {};
        std::snprintf(fixedBuffer, sizeof(fixedBuffer), "%s", project_.fixedCsvPath.c_str());
        if (UI::InputText("固定CSV", fixedBuffer, sizeof(fixedBuffer))) {
            project_.fixedCsvPath = fixedBuffer;
            RebuildBrowseList();
        }

        UI::SectionHeader("エリア");

        const float listHeight = ImGui::GetTextLineHeightWithSpacing() * 5.0f;
        if (ImGui::BeginListBox("##StageAreaList", ImVec2(-FLT_MIN, listHeight))) {
            for (int i = 0; i < static_cast<int>(project_.areas.size()); ++i) {
                const auto& area = project_.areas[i];
                char label[128] = {};
                std::snprintf(label, sizeof(label), "%s  (%zu 区画)",
                    area.name.c_str(), area.paths.size());
                if (ImGui::Selectable(label, i == selectedAreaIndex_)) {
                    selectedAreaIndex_ = i;
                }
            }
            ImGui::EndListBox();
        }

        ImGui::SetNextItemWidth(180.0f);
        UI::InputText("新しいエリア名", newAreaBuffer_, sizeof(newAreaBuffer_));
        UI::SameLine();
        if (ImGui::Button("エリアを追加")) {
            const std::string name = newAreaBuffer_;
            if (name.empty()) {
                SetStatus("エリア名が空です", true);
            } else if (StageProjectIO::FindArea(project_, name) != nullptr) {
                SetStatus("同じ名前のエリアがあります: " + name, true);
            } else {
                // CSVを置く先を先に用意しておく。空のままでもプールとしては有効。
                std::error_code ec;
                const std::string directory = std::string(StageProjectIO::kAreasRoot) + "/" + name;
                std::filesystem::create_directories(
                    std::filesystem::path(std::u8string(directory.begin(), directory.end())), ec);
                project_.areas.push_back({ name, {} });
                selectedAreaIndex_ = static_cast<int>(project_.areas.size()) - 1;
                newAreaBuffer_[0] = '\0';
                RebuildBrowseList();
                SetStatus("エリアを追加しました: " + name);
            }
        }

        const bool hasArea = selectedAreaIndex_ >= 0
            && selectedAreaIndex_ < static_cast<int>(project_.areas.size());
        ImGui::BeginDisabled(!hasArea);
        if (ImGui::Button("選択エリアを構成から外す")) {
            SetStatus("エリアを外しました: " + project_.areas[selectedAreaIndex_].name);
            project_.areas.erase(project_.areas.begin() + selectedAreaIndex_);
            selectedAreaIndex_ = project_.areas.empty() ? -1 : 0;
            RebuildBrowseList();
        }
        ImGui::EndDisabled();
        UI::SameLine();
        UI::HelpMarker("構成から外すだけで、CSVファイルとフォルダーは消しません。");

        if (hasArea) {
            StageAreaDefinition& area = project_.areas[selectedAreaIndex_];
            UI::SectionHeader(("区画CSV: " + area.name).c_str());

            int removeIndex = -1;
            for (int i = 0; i < static_cast<int>(area.paths.size()); ++i) {
                ImGui::PushID(i);
                if (ImGui::SmallButton("外す")) {
                    removeIndex = i;
                }
                UI::SameLine();
                ImGui::TextUnformatted(FileNameOf(area.paths[i]).c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", area.paths[i].c_str());
                }
                ImGui::PopID();
            }
            if (removeIndex >= 0) {
                area.paths.erase(area.paths.begin() + removeIndex);
                RebuildBrowseList();
            }

            ImGui::SetNextItemWidth(180.0f);
            UI::InputText("新しい区画名", newCsvBuffer_, sizeof(newCsvBuffer_));
            UI::SameLine();
            if (ImGui::Button("区画CSVを作って追加")) {
                const std::string name = newCsvBuffer_;
                if (name.empty()) {
                    SetStatus("区画名が空です", true);
                } else {
                    const std::string path = std::string(StageProjectIO::kAreasRoot)
                        + "/" + area.name + "/" + name + ".csv";
                    StageCsvDocument fresh;
                    fresh.Reset(project_.chunkSizeX, project_.mapSizeZ,
                        GameComponents::MapChipType::Ground);
                    if (fresh.Save(path)) {
                        area.paths.push_back(path);
                        newCsvBuffer_[0] = '\0';
                        RebuildBrowseList();
                        OpenCsv(path);
                    } else {
                        SetStatus("区画CSVを作れません: " + path, true);
                    }
                }
            }
        }

        UI::Separator();
        if (ImGui::Button("構成を保存")) {
            if (StageProjectIO::Save(projectPath_, project_)) {
                SetStatus("ステージ構成を保存しました: " + projectPath_);
            } else {
                SetStatus("ステージ構成を保存できません: " + projectPath_, true);
            }
        }
        UI::SameLine();
        if (ImGui::Button("構成を読み直す")) {
            ReloadProject();
        }
        UI::SameLine();
        if (ImGui::Button("フォルダーから作り直す")) {
            project_ = StageProjectIO::ScanFromDisk(StageProjectIO::kAreasRoot);
            selectedAreaIndex_ = project_.areas.empty() ? -1 : 0;
            RebuildBrowseList();
            SetStatus("エリアフォルダーの中身から構成を作り直しました");
        }

        UI::SectionHeader("GameScene へ反映する");
        ImGui::TextWrapped(
            "ゲーム本体はこの構成ファイルを読みません。エリアの増減をゲームへ効かせるときは、"
            "下のコードを GameScene::OnInitialize の mapSettings と差し替えてください。");

        const std::string snippet = StageProjectIO::BuildGameSceneSnippet(project_);
        snippetBuffer_.assign(snippet.begin(), snippet.end());
        snippetBuffer_.push_back('\0');
        ImGui::InputTextMultiline("##StageSnippet", snippetBuffer_.data(), snippetBuffer_.size(),
            ImVec2(-FLT_MIN, 170.0f), ImGuiInputTextFlags_ReadOnly);
        if (ImGui::Button("コードをコピー")) {
            ImGui::SetClipboardText(snippet.c_str());
            SetStatus("クリップボードへコピーしました");
        }
    }
}

#endif // USE_IMGUI

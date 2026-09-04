#pragma once

#ifdef USE_IMGUI

#include <cstddef>
#include <string>
#include <vector>

namespace GameEditors
{
    /// @brief 1エリアぶんの区画CSV集合（MapGenerationSettings の csvPools と同じ形）
    struct StageAreaDefinition {
        std::string name;
        std::vector<std::string> paths;
    };

    /// @brief ステージ全体の構成
    /// @details GameScene::OnInitialize が組み立てている mapSettings と同じ内容を持つ。
    ///          ゲーム側はいまもコード内の設定で動くので、こちらは「エディタが扱う構成表」
    ///          として使う。GameScene へ反映したいときは BuildGameSceneSnippet() の
    ///          出力を貼り付ける。
    struct StageProject {
        std::size_t chunkSizeX = 10;                                    ///< ランダム区画のX幅
        std::size_t mapSizeZ = 9;                                       ///< マップのZ方向マス数
        std::string initialAreaName = "Area1";                          ///< 開始エリア
        std::string fixedCsvPath = "Application/Assets/Maps/fixed.csv"; ///< 固定CSV方式で使う1枚
        std::vector<StageAreaDefinition> areas;
    };

    /// @brief ステージ構成ファイルの読み書き
    namespace StageProjectIO
    {
        /// @brief 構成ファイルの既定パス（実行時のカレントディレクトリ基準）
        inline constexpr const char* kDefaultPath = "Application/Assets/Maps/stage_project.json";

        /// @brief エリアフォルダーを置く場所
        inline constexpr const char* kAreasRoot = "Application/Assets/Maps/Areas";

        /// @brief 構成ファイルを読み込む
        /// @param path 読み込むJSONのパス
        /// @param out 読み込み先。失敗時は変更しない
        /// @return 読み込めた場合 true
        bool Load(const std::string& path, StageProject& out);

        /// @brief 構成ファイルを書き出す
        /// @return 書き出せた場合 true
        bool Save(const std::string& path, const StageProject& project);

        /// @brief エリアフォルダーを走査して構成を組み立てる
        /// @param areasRoot エリアフォルダーの親（kAreasRoot）
        /// @details 構成ファイルがまだ無いときの初期値に使う。
        ///          フォルダー名がエリア名、その直下の .csv が区画になる。
        StageProject ScanFromDisk(const std::string& areasRoot);

        /// @brief GameScene::OnInitialize へ貼り付けられる形の設定コードを作る
        /// @details ゲーム本体は構成JSONを読まないので、反映したいときはこれを貼る。
        std::string BuildGameSceneSnippet(const StageProject& project);

        /// @brief エリアを名前で探す
        /// @return 見つからなければ nullptr
        const StageAreaDefinition* FindArea(const StageProject& project, const std::string& name);
    }
}

#endif // USE_IMGUI

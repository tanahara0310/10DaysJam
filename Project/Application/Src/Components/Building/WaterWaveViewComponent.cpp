#include "pch.h"
#include "WaterWaveViewComponent.h"

#include "Camera/Camera.h"
#include "EngineSystem/EngineSystem.h"
#include "GameObject/GameObject.h"
#include "GameObject/Component/Render/MeshRendererComponent.h"
#include "GameObject/Component/Transform/TransformComponent.h"
#include "Graphics/Material/MaterialBase.h"
#include "Graphics/Material/MaterialInstance.h"
#include "Graphics/Pipeline/CustomShaderPipeline.h"
#include "Graphics/Primitive/PlaneMeshGenerator.h"
#include "Graphics/RHI/GraphicsCore.h"
#include "Graphics/RootSignature/ShaderBinder.h"
#include "Graphics/Shader/ICustomShaderProvider.h"
#include "Graphics/Water/Surface/WaterSurfaceTypes.h"
#include "MapGeneratorComponent.h"
#include "Components/Utility/BlockModelLayout.h"
#include "Utility/FrameRate/Time.h"
#include "Utility/Logger/Logger.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>

#ifdef USE_IMGUI
#include "Editor/ImGui/ImGuiAll.h"
#endif

using namespace CoreEngine;

namespace GameComponents
{
    /// @brief 水面の板へ差すカスタムシェーダー一式
    /// @details 頂点シェーダーだけ独自にして、ピクセルシェーダーは既定のまま使う。
    ///          こうすると通常のモデルと同じライティング・影・フォグにそのまま乗る。
    /// @note 波の定数バッファは MaterialBase が確保・マップする1本を使い回す。
    ///       全ての板が同じ波を評価するので、板ごとにバッファを持つ必要はない。
    class WaterWaveShaderProvider final
        : public CoreEngine::ICustomShaderProvider,
          public CoreEngine::MaterialBase<WaterConstants> {
    public:
        void Initialize(ID3D12Device* device) { InitializeBuffer(device); }

        bool IsReady() const { return materialData_ != nullptr; }

        /// @brief 今フレームの波を GPU へ書き込む
        void Upload(const WaterConstants& constants) {
            if (!materialData_) { return; }
            *materialData_ = constants;
        }

        std::wstring GetVertexShaderPath() const override { return L"WaterWave.VS.hlsl"; }
        // ライティングは既定のフォワード PS に任せる（水専用の見た目は色と粗さで作る）
        std::wstring GetPixelShaderPath() const override { return L"Object3d.PS.hlsl"; }
        // 板は裏からも見える（水面より低い位置にカメラが来る）ので両面描く
        D3D12_CULL_MODE GetCullMode() const override { return D3D12_CULL_MODE_NONE; }

        void BindCustomResources(
            ID3D12GraphicsCommandList* cmdList,
            const CoreEngine::CustomShaderPipeline* pipeline) const override {
            if (!cmdList || !pipeline || !materialData_) { return; }

            EnsureResolved(pipeline);
            ShaderBinder binder(cmdList, ShaderBinder::Pipeline::Graphics);
            binder.Set(waveConstantsSlot_, GetGPUVirtualAddress());
        }

    private:
        /// @brief ルートパラメータ番号をリフレクション結果から引き当てる
        /// @note b8 という番号を C++ 側へ直書きしないこと。シェーダーを書き換えた
        ///       ときに片方だけずれて、静かに別のリソースを潰す事故になる。
        void EnsureResolved(const CoreEngine::CustomShaderPipeline* pipeline) const {
            const void* rootSignature = pipeline->GetForwardRootSignature();
            if (rootSignature == resolvedRootSignature_) { return; }

            // cbuffer 宣言のブロック名がそのままリソース名になる
            waveConstantsSlot_ = pipeline->GetRootSlot("WaterConstants");
            resolvedRootSignature_ = rootSignature;
        }

        // BindCustomResources は const 契約なので、解決結果のキャッシュは mutable で持つ
        mutable RootSlot waveConstantsSlot_{};
        mutable const void* resolvedRootSignature_ = nullptr;
    };
}

namespace {
    /// @brief 水面の描画順
    /// @details RenderManager::ResetPassTypePriorities() の並びに合わせた値。
    ///          SkyBox=300 の後、ModelParticle=400・Sprite=700・UI=800 より前。
    ///          エンジン純正の水面パス（WaterSurface）と同じ位置に置いている。
    constexpr int kWaterRenderOrder = 350;

    /// @brief 板 1 マスあたりの分割数
    /// @details 1 マス 1m なら頂点間隔 0.167m。最短波長 0.95m でも 5 頂点以上で拾える。
    ///          上げるほど滑らかになるが、頂点数は 2 乗で増える。
    constexpr uint32_t kPlaneSubdivision = 6;

    /// @brief 重ね合わせる Gerstner 波 1 本ぶんの設定（1 マス = 1m を基準にした値）
    struct WaterWavePreset {
        float directionX;
        float directionZ;
        float wavelength; ///< 波長[m]
        float amplitude;  ///< 振幅[m]
        float speed;      ///< 位相速度[m/s]
        float steepness;  ///< 0 で純粋な上下。大きいほど山が尖る
    };

    /// @brief 波長も向きも意図的に揃えていない。
    /// @details 揃えると位相が全マスで一致し、「水面が一斉に上下している」ように見える。
    ///          互いに素に近い波長を重ねると、繰り返しの周期が長くなって自然になる。
    /// @note 見た目に効くのは振幅そのものより「振幅 ÷ 波長」＝斜面の傾き。
    ///       高さを出さずに波を見せたいときは、振幅を上げるより波長を詰めるほうが安全。
    ///       振幅の合計は必ず水面から地面の上面までの余裕（1 マスの 0.35 倍）に収めること。
    ///       超えると波の山が岸へ乗り上げて見える。
    /// @note steepness（横ずれ）は必ず 0 にすること。
    ///       Gerstner 本来の横ずれは頂点を XZ 方向へも動かすが、ここは水域を
    ///       1 マス 1 枚の板で敷き詰めている。板の縁が自分のマスの外へはみ出すと、
    ///       隣の地面ブロックの側面を突き抜けて水が岸へ乗り上げて見える。
    ///       上下だけの単純な波にすれば、板は絶対に自分のマスから出ない。
    constexpr WaterWavePreset kWaterWavePresets[] = {
        {  0.970f,  0.242f, 3.20f, 0.048f, 0.55f, 0.0f },
        { -0.371f,  0.928f, 2.10f, 0.027f, 0.45f, 0.0f },
        {  0.707f, -0.707f, 1.35f, 0.013f, 0.65f, 0.0f },
        {  0.259f,  0.966f, 0.95f, 0.006f, 0.85f, 0.0f },
    };
    constexpr uint32_t kWaterWaveCount =
        static_cast<uint32_t>(std::size(kWaterWavePresets));
    static_assert(kWaterWaveCount <= kMaxWaterWaveCount,
        "定数バッファに入る波の本数を超えている");

    /// @brief 位置から安定した割り当てキーを作る
    /// @details ModelRenderPoolComponent と同じ方式。XZ を 0.01 単位へ量子化して
    ///          64bit へ詰める。マップ 1 マスは必ず同じ座標で描かれるので、
    ///          これで毎フレーム同じキーになる。
    std::uint64_t MakePositionKey(float worldX, float worldZ) {
        const auto x = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(std::llround(worldX * 100.0)));
        const auto z = static_cast<std::uint32_t>(
            static_cast<std::int32_t>(std::llround(worldZ * 100.0)));
        return (static_cast<std::uint64_t>(x) << 32) | static_cast<std::uint64_t>(z);
    }
}

GameComponents::WaterWaveViewComponent::WaterWaveViewComponent(
    MapGeneratorComponent* mapGenerator,
    CoreEngine::Camera* viewCamera,
    float gridSize,
    uint32_t viewDistanceX,
    std::size_t initialCapacity)
    : gridSize_(gridSize),
      viewDistanceX_(viewDistanceX),
      initialCapacity_(initialCapacity),
      mapGenerator_(mapGenerator),
      viewCamera_(viewCamera) {
}

// unique_ptr が指す型の定義がここまで来ないとデストラクタを作れないので、
// 宣言はヘッダ・定義はこちらに置く
GameComponents::WaterWaveViewComponent::~WaterWaveViewComponent() = default;

json GameComponents::WaterWaveViewComponent::OnSerialize() const {
    return {
        { "gridSize", gridSize_ },
        { "viewDistanceX", viewDistanceX_ },
        { "waveHeightScale", waveHeightScale_ },
        { "waveSpeedScale", waveSpeedScale_ },
        { "waterLevelRatio", waterLevelRatio_ },
        { "waterRoughness", waterRoughness_ },
        { "waterColor", JsonManager::Vector4ToJson(waterColor_) }
    };
}

void GameComponents::WaterWaveViewComponent::OnDeserialize(const json& j) {
    gridSize_ = std::max(0.01f, JsonManager::SafeGet<float>(j, "gridSize", gridSize_));
    viewDistanceX_ = std::max<uint32_t>(1,
        JsonManager::SafeGet<uint32_t>(j, "viewDistanceX", viewDistanceX_));
    waveHeightScale_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "waveHeightScale", waveHeightScale_));
    waveSpeedScale_ = std::max(0.0f,
        JsonManager::SafeGet<float>(j, "waveSpeedScale", waveSpeedScale_));
    waterLevelRatio_ = std::clamp(
        JsonManager::SafeGet<float>(j, "waterLevelRatio", waterLevelRatio_), 0.0f, 1.0f);
    waterColor_ = JsonManager::SafeGetVector4(j, "waterColor", waterColor_);
    waterRoughness_ = std::clamp(
        JsonManager::SafeGet<float>(j, "waterRoughness", waterRoughness_), 0.0f, 1.0f);
    ApplyMaterialToEntries();
}

#ifdef USE_IMGUI
bool GameComponents::WaterWaveViewComponent::DrawInspector() {
    bool changed = false;
    changed |= ImGui::DragFloat("波の高さ", &waveHeightScale_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat("波の速さ", &waveSpeedScale_, 0.01f, 0.0f, 5.0f);
    changed |= ImGui::DragFloat("水面の高さ", &waterLevelRatio_, 0.005f, 0.0f, 1.0f);

    // || で繋ぐと短絡して後ろのウィジェットが描かれなくなるので、必ず別々に呼ぶ
    const bool colorEdited = ImGui::ColorEdit4("色", &waterColor_.x);
    const bool roughnessEdited =
        ImGui::DragFloat("粗さ", &waterRoughness_, 0.005f, 0.0f, 1.0f);
    if (colorEdited || roughnessEdited) {
        ApplyMaterialToEntries();
        changed = true;
    }

    ImGui::TextDisabled("板: %zu 枚", entries_.size());
    return changed;
}
#endif

void GameComponents::WaterWaveViewComponent::Awake() {
    GameObject* owner = GetOwner();
    EngineSystem* engine = owner ? owner->GetEngineSystem() : nullptr;
    auto* graphics = engine ? engine->GetService<GraphicsCore>() : nullptr;
    if (!graphics || !graphics->GetDevice()) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "WaterWaveViewComponent: 描画デバイスを取得できませんでした");
        SetEnabled(false);
        return;
    }

    shaderProvider_ = std::make_unique<WaterWaveShaderProvider>();
    shaderProvider_->Initialize(graphics->GetDevice());
    if (!shaderProvider_->IsReady()) {
        Logger::GetInstance().Errorf(
            LogCategory::Game,
            "WaterWaveViewComponent: 波の定数バッファを確保できませんでした");
        SetEnabled(false);
        return;
    }
    UploadWaveConstants();

    entries_.reserve(initialCapacity_);
    while (entries_.size() < initialCapacity_) {
        if (!CreateEntry()) {
            Logger::GetInstance().Errorf(
                LogCategory::Game,
                "WaterWaveViewComponent: 板の事前生成に失敗しました ({}/{})",
                entries_.size(), initialCapacity_);
            SetEnabled(false);
            return;
        }
    }
}

void GameComponents::WaterWaveViewComponent::Update() {
    if (mapGenerator_ == nullptr || viewCamera_ == nullptr || !shaderProvider_) {
        return;
    }

    // 波の時間を進めて GPU へ送る。板は動かさず、頂点シェーダーがこの時刻で変位させる。
    elapsedTime_ += Time::DeltaTime();
    UploadWaveConstants();

    // 描画範囲の決め方は MapViewComponent と揃える（地面と水がずれた範囲で出ないように）
    const auto cameraFocusPosition = viewCamera_->GetTranslate();
    const float cameraFocusGridX = std::round(cameraFocusPosition.x / gridSize_);
    const uint32_t viewCenterX = cameraFocusGridX > 0.0f
        ? static_cast<uint32_t>(cameraFocusGridX)
        : 0;

    const std::size_t startX = (viewCenterX > viewDistanceX_)
        ? (viewCenterX - viewDistanceX_)
        : 0;
    const std::size_t endX = viewCenterX + viewDistanceX_;

    // カメラの先に必要な分だけ、X正方向へマップを延長する
    mapGenerator_->CreateToX(endX);

    const std::uint64_t frame = Time::FrameCount();
    BeginFrameIfNeeded(frame);

    const auto& mapChips = mapGenerator_->GetMapChips();
    for (std::size_t x = startX; x < endX && x < mapChips.size(); ++x) {
        for (std::size_t z = 0; z < mapChips[x].size(); ++z) {
            if (mapGenerator_->GetMapChip(x, z) != MapChipType::Water) {
                continue;
            }
            DrawCell(x * gridSize_, z * gridSize_, frame);
        }
    }

    // このフレームに配られなかった板は隠す
    for (Entry& entry : entries_) {
        if (!entry.object || entry.object->IsMarkedForDestroy()) {
            continue;
        }
        if (entry.lastSubmittedFrame != frame && entry.object->IsActive()) {
            entry.object->SetActive(false);
        }
    }
}

void GameComponents::WaterWaveViewComponent::OnDestroy() {
    for (Entry& entry : entries_) {
        if (entry.object && !entry.object->IsMarkedForDestroy()) {
            entry.object->Destroy();
        }
    }
    entries_.clear();
    entryByPosition_.clear();
    prevEntryByPosition_.clear();
}

GameComponents::WaterWaveViewComponent::Entry*
GameComponents::WaterWaveViewComponent::CreateEntry() {
    GameObject* owner = GetOwner();
    if (!owner || !shaderProvider_) {
        return nullptr;
    }

    GameObject* object = owner->Spawn<GameObject>();
    if (!object) {
        return nullptr;
    }

    object->SetName(
        owner->GetName() + "_WaterPlane_" + std::to_string(entries_.size()));
    object->SetSerializeEnabled(false);
    // 描画順は明示的に指定する。RenderManager::ResolveRenderOrder() は
    // ブレンド有りのモデルへ一律 +10000 するので、放っておくと
    // Model(100)+10000 = 10100 となり Sprite(700) や UI(800) より後、
    // つまりポーズメニューの手前へ水が出てしまう。
    // エンジンの水面パス（WaterSurface）と同じ 350 に置き、
    // 空（300）の後・パーティクル（400）や UI より前で描く。
    object->SetRenderOrder(kWaterRenderOrder);

    TransformComponent* transform = object->AddComponent<TransformComponent>();

    // メッシュ未指定で載せてから設定する。AddComponent の中で Awake() が走るので、
    // 先にメッシュを渡すとカスタムシェーダー指定より前に PSO が組まれてしまう。
    auto* renderer = object->AddComponent<MeshRendererComponent>();
    // ブレンド無しのモデルは Deferred 経路へ振り分けられ、その経路には
    // カスタム PSO を差す口が無い（＝波が消えて平らな板になる）。
    // ブレンドありにしてフォワード経路へ乗せることが、波を出すための前提条件。
    // α は 1 のままでよく、見た目は不透明のまま。
    renderer->SetBlendMode(BlendMode::kBlendModeNormal);
    renderer->SetCustomShaderProvider(shaderProvider_.get());
    // 板はローカル 1×1。頂点変位はワールド座標で決まるので、
    // マスごとに別の板でも隣と縁の高さが必ず一致する。
    renderer->SetPrimitive(std::make_unique<PlaneMeshGenerator>(
        1.0f, 1.0f, kPlaneSubdivision, kPlaneSubdivision));
    renderer->ReloadFromSpec();

    object->SetActive(false);
    entries_.push_back({ object, transform, renderer });

    // マテリアルは MaterialComponent を介さず、モデルへ直接入れる。
    // MaterialComponent は Start() で「α が 1 ならブレンド無し」へ戻してしまい、
    // 上でせっかく指定したフォワード経路が Deferred へ落ちて波が止まるため。
    ApplyMaterialToEntry(entries_.back());
    return &entries_.back();
}

void GameComponents::WaterWaveViewComponent::BeginFrameIfNeeded(std::uint64_t frame) {
    if (allocationFrame_ == frame) {
        return;
    }
    allocationFrame_ = frame;
    nextEntryIndex_ = 0;
    prevEntryByPosition_ = std::move(entryByPosition_);
    entryByPosition_.clear();
}

void GameComponents::WaterWaveViewComponent::DrawCell(
    float worldX, float worldZ, std::uint64_t frame) {
    const std::uint64_t positionKey = MakePositionKey(worldX, worldZ);

    // 前フレームに同じマスを描いた板を優先して使い回す（担当がずれると TAA がぶれる）
    Entry* entry = nullptr;
    if (const auto it = prevEntryByPosition_.find(positionKey);
        it != prevEntryByPosition_.end() && it->second < entries_.size()) {
        Entry& candidate = entries_[it->second];
        if (candidate.lastSubmittedFrame != frame && candidate.object &&
            !candidate.object->IsMarkedForDestroy()) {
            entry = &candidate;
        }
    }

    while (entry == nullptr && nextEntryIndex_ < entries_.size()) {
        Entry& candidate = entries_[nextEntryIndex_++];
        if (candidate.lastSubmittedFrame != frame && candidate.object &&
            !candidate.object->IsMarkedForDestroy()) {
            entry = &candidate;
        }
    }

    if (entry == nullptr) {
        entry = CreateEntry();
    }

    if (entry == nullptr || !entry->object || !entry->transform) {
        // 生成にも失敗した場合。警告は1フレームに1回までに抑える。
        if (lastExhaustedWarningFrame_ != frame) {
            lastExhaustedWarningFrame_ = frame;
            Logger::GetInstance().Warnf(
                LogCategory::Game,
                "WaterWaveViewComponent: 板が足りません (capacity={})",
                entries_.size());
        }
        return;
    }

    auto& transform = entry->transform->Get();
    transform.translate = { worldX, GetWaterSurfaceHeight(), worldZ };
    transform.rotate = { 0.0f, 0.0f, 0.0f };
    transform.scale = { gridSize_, 1.0f, gridSize_ };
    transform.TransferMatrix();

    entry->lastSubmittedFrame = frame;
    entry->object->SetActive(true);
    entryByPosition_[positionKey] =
        static_cast<std::size_t>(entry - entries_.data());
}

void GameComponents::WaterWaveViewComponent::UploadWaveConstants() {
    if (!shaderProvider_) {
        return;
    }

    WaterConstants constants{};
    constants.activeWaveCount = kWaterWaveCount;
    constants.time = elapsedTime_;
    for (uint32_t i = 0; i < kWaterWaveCount; ++i) {
        const WaterWavePreset& preset = kWaterWavePresets[i];
        WaveParams& wave = constants.waves[i];
        wave.direction = { preset.directionX, preset.directionZ };
        // 波の形はマスの大きさへ追従させる。gridSize を変えても粒度の印象が変わらない。
        wave.wavelength = preset.wavelength * gridSize_;
        wave.amplitude = preset.amplitude * gridSize_ * waveHeightScale_;
        wave.speed = preset.speed * gridSize_ * waveSpeedScale_;
        wave.steepness = preset.steepness;
        wave.phaseOffset = 0.0f;
    }

    shaderProvider_->Upload(constants);
}

void GameComponents::WaterWaveViewComponent::ApplyMaterialToEntry(Entry& entry) {
    Model* model = entry.renderer ? entry.renderer->GetModel() : nullptr;
    if (!model) {
        return;
    }
    model->ForEachMaterial([this](MaterialInstance* material) {
        material->SetColor(waterColor_);
        material->SetMetallic(0.0f);
        material->SetRoughness(waterRoughness_);
        // ディザリングが有効だと半透明にしたときアルファテスト扱いになる。
        // ここは常にブレンドで出したいので切っておく。
        material->SetDitheringEnabled(false);
    });
}

void GameComponents::WaterWaveViewComponent::ApplyMaterialToEntries() {
    for (Entry& entry : entries_) {
        if (!entry.object || entry.object->IsMarkedForDestroy()) {
            continue;
        }
        ApplyMaterialToEntry(entry);
    }
}

float GameComponents::WaterWaveViewComponent::GetWaterSurfaceHeight() const {
    // 地面ブロックは [底面, 底面+1マス] を占める。その途中に静止水面を置く。
    return BlockModelLayout::GetGroundHeight(gridSize_) + waterLevelRatio_ * gridSize_;
}

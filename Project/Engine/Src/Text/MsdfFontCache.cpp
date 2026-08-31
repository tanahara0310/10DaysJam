#include "pch.h"
#include "Text/MsdfFontCache.h"

#include "Text/MsdfFont.h"
#include "Utility/Logger/Logger.h"

#include <cstdio>
#include <cstring>
#include <format>

namespace CoreEngine
{
    namespace
    {
        /// キャッシュ形式の版。構造を変えたら必ず上げること
        /// （上げ忘れると古いキャッシュを新しい版として読んで壊れる）
        constexpr uint32_t kCacheVersion = 1;
        constexpr char kCacheMagic[8] = { 'M','S','D','F','A','T','L','\0' };

        /// @brief キャッシュファイル先頭の固定長ヘッダ
        struct CacheHeader
        {
            char     magic[8];
            uint32_t version;
            uint32_t glyphStructSize; ///< MsdfGlyph のサイズ。ビルドが変わった検出用
            int32_t  atlasWidth;
            int32_t  atlasHeight;
            int32_t  glyphPixelSize;
            float    pxRange;
            int32_t  padding;
            float    ascender;
            float    descender;
            float    lineHeight;
            uint32_t glyphCount;
            uint32_t pixelByteCount;
        };

        // ── FNV-1a 64bit ────────────────────────────────────────
        constexpr uint64_t kFnvOffsetBasis = 1469598103934665603ULL;
        constexpr uint64_t kFnvPrime = 1099511628211ULL;

        void HashBytes(uint64_t& hash, const void* data, size_t size)
        {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < size; ++i) {
                hash ^= bytes[i];
                hash *= kFnvPrime;
            }
        }

        template <class T>
        void HashValue(uint64_t& hash, const T& value)
        {
            static_assert(std::is_trivially_copyable_v<T>);
            HashBytes(hash, &value, sizeof(T));
        }

        /// @brief RAII で閉じる FILE*
        struct FileHandle
        {
            std::FILE* file = nullptr;
            ~FileHandle() { if (file) { std::fclose(file); } }
            explicit operator bool() const { return file != nullptr; }
        };
    } // namespace

    uint64_t MsdfFontCache::ComputeKey(const MsdfFontDesc& desc,
        const std::vector<std::wstring>& resolvedChain)
    {
        uint64_t hash = kFnvOffsetBasis;

        HashValue(hash, kCacheVersion);

        // 実際に開けたフォント名（順序込み）
        for (const std::wstring& name : resolvedChain) {
            HashBytes(hash, name.data(), name.size() * sizeof(wchar_t));
            HashValue(hash, '|');
        }

        // ファイル指定のフォントは中身が差し替わることがあるので更新時刻も混ぜる
        if (!desc.filePath.empty()) {
            std::error_code ec;
            const auto writeTime = std::filesystem::last_write_time(desc.filePath, ec);
            if (!ec) {
                const auto ticks = writeTime.time_since_epoch().count();
                HashValue(hash, ticks);
            }
            HashValue(hash, desc.faceIndex);
        }

        HashValue(hash, desc.bake.glyphPixelSize);
        HashValue(hash, desc.bake.pxRange);
        HashValue(hash, desc.bake.atlasWidth);
        HashValue(hash, desc.bake.atlasHeight);
        HashValue(hash, desc.bake.padding);
        HashValue(hash, desc.includeAscii);

        HashBytes(hash, desc.charsetUtf8.data(), desc.charsetUtf8.size());

        return hash;
    }

    std::filesystem::path MsdfFontCache::MakePath(const std::filesystem::path& directory, uint64_t key)
    {
        return directory / std::format("msdf_{:016x}.bin", key);
    }

    bool MsdfFontCache::TryLoad(const std::filesystem::path& path, MsdfBakeResult& outResult)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            return false;
        }

        FileHandle handle;
        if (_wfopen_s(&handle.file, path.c_str(), L"rb") != 0 || !handle) {
            return false;
        }

        CacheHeader header{};
        if (std::fread(&header, sizeof(header), 1, handle.file) != 1) {
            return false;
        }

        // 版・形式が違うものは黙って捨てて焼き直す（古い内容を読むより安全）
        if (std::memcmp(header.magic, kCacheMagic, sizeof(kCacheMagic)) != 0
            || header.version != kCacheVersion
            || header.glyphStructSize != sizeof(MsdfGlyph)) {
            return false;
        }

        const size_t expectedPixels =
            static_cast<size_t>(header.atlasWidth) * header.atlasHeight * 4;
        if (header.atlasWidth <= 0 || header.atlasHeight <= 0
            || header.pixelByteCount != expectedPixels) {
            return false;
        }

        MsdfBakeResult loaded{};
        loaded.atlasWidth = header.atlasWidth;
        loaded.atlasHeight = header.atlasHeight;
        loaded.settings.glyphPixelSize = header.glyphPixelSize;
        loaded.settings.pxRange = header.pxRange;
        loaded.settings.atlasWidth = header.atlasWidth;
        loaded.settings.atlasHeight = header.atlasHeight;
        loaded.settings.padding = header.padding;
        loaded.metrics.ascender = header.ascender;
        loaded.metrics.descender = header.descender;
        loaded.metrics.lineHeight = header.lineHeight;

        if (std::fread(&loaded.notdefGlyph, sizeof(MsdfGlyph), 1, handle.file) != 1) {
            return false;
        }

        loaded.glyphs.reserve(header.glyphCount);
        for (uint32_t i = 0; i < header.glyphCount; ++i) {
            uint32_t codePoint = 0;
            MsdfGlyph glyph{};
            if (std::fread(&codePoint, sizeof(codePoint), 1, handle.file) != 1
                || std::fread(&glyph, sizeof(glyph), 1, handle.file) != 1) {
                return false;
            }
            loaded.glyphs[static_cast<char32_t>(codePoint)] = glyph;
            if (glyph.hasBitmap) { ++loaded.bakedGlyphCount; }
        }

        loaded.pixels.resize(header.pixelByteCount);
        if (std::fread(loaded.pixels.data(), 1, header.pixelByteCount, handle.file)
            != header.pixelByteCount) {
            return false;
        }

        loaded.success = true;
        outResult = std::move(loaded);
        return true;
    }

    bool MsdfFontCache::Save(const std::filesystem::path& path, const MsdfBakeResult& result)
    {
        if (!result.success || result.pixels.empty()) {
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        // 途中で落ちた書き込みを正規のキャッシュとして読まないよう、
        // 一時ファイルへ書いてから差し替える
        std::filesystem::path tempPath = path;
        tempPath += ".tmp";

        {
            FileHandle handle;
            if (_wfopen_s(&handle.file, tempPath.c_str(), L"wb") != 0 || !handle) {
                Logger::GetInstance().Logf(LogLevel::Warn, LogCategory::Resource,
                    "MsdfFontCache: 書き込み先を開けませんでした: {}",
                    Logger::GetInstance().PathToUtf8(tempPath));
                return false;
            }

            CacheHeader header{};
            std::memcpy(header.magic, kCacheMagic, sizeof(kCacheMagic));
            header.version = kCacheVersion;
            header.glyphStructSize = sizeof(MsdfGlyph);
            header.atlasWidth = result.atlasWidth;
            header.atlasHeight = result.atlasHeight;
            header.glyphPixelSize = result.settings.glyphPixelSize;
            header.pxRange = result.settings.pxRange;
            header.padding = result.settings.padding;
            header.ascender = result.metrics.ascender;
            header.descender = result.metrics.descender;
            header.lineHeight = result.metrics.lineHeight;
            header.glyphCount = static_cast<uint32_t>(result.glyphs.size());
            header.pixelByteCount = static_cast<uint32_t>(result.pixels.size());

            bool ok = std::fwrite(&header, sizeof(header), 1, handle.file) == 1;
            ok = ok && std::fwrite(&result.notdefGlyph, sizeof(MsdfGlyph), 1, handle.file) == 1;

            for (const auto& [codePoint, glyph] : result.glyphs) {
                const uint32_t cp = static_cast<uint32_t>(codePoint);
                ok = ok && std::fwrite(&cp, sizeof(cp), 1, handle.file) == 1;
                ok = ok && std::fwrite(&glyph, sizeof(glyph), 1, handle.file) == 1;
            }

            ok = ok && std::fwrite(result.pixels.data(), 1, result.pixels.size(), handle.file)
                == result.pixels.size();

            if (!ok) {
                Logger::GetInstance().Logf(LogLevel::Warn, LogCategory::Resource,
                    "MsdfFontCache: 書き込みに失敗しました: {}",
                    Logger::GetInstance().PathToUtf8(tempPath));
                return false;
            }
        }

        std::filesystem::rename(tempPath, path, ec);
        if (ec) {
            std::filesystem::remove(tempPath, ec);
            return false;
        }

        return true;
    }
}

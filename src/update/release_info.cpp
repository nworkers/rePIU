#include "repiu/update/release_info.h"

#include <charconv>
#include <cstddef>
#include <utility>

namespace repiu::update
{
namespace
{

// A JSON reader just big enough for the release response: it keeps strings,
// objects, arrays and the text of numbers, and skips nothing silently. The
// response nests the author and uploader objects, so this is a real parser
// rather than a search for keys.
struct JsonValue
{
    enum class Kind
    {
        kNull,
        kBool,
        kNumber,
        kString,
        kArray,
        kObject,
    };
    Kind kind = Kind::kNull;
    bool boolean = false;
    std::string text;  // A string's value, or a number's literal.
    std::vector<JsonValue> items;
    std::vector<std::pair<std::string, JsonValue>> members;

    const JsonValue* Member(std::string_view key) const
    {
        if (kind != Kind::kObject)
        {
            return nullptr;
        }
        for (const auto& member : members)
        {
            if (member.first == key)
            {
                return &member.second;
            }
        }
        return nullptr;
    }
};

class JsonParser
{
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    bool Parse(JsonValue* value, std::string* error)
    {
        if (!ParseValue(value, 0))
        {
            *error = "malformed JSON at offset " + std::to_string(position_);
            return false;
        }
        SkipSpace();
        if (position_ != text_.size())
        {
            *error = "trailing data after JSON at offset " +
                std::to_string(position_);
            return false;
        }
        return true;
    }

private:
    // GitHub's response nests a few levels; anything far deeper is not it.
    static constexpr int kMaxDepth = 64;

    void SkipSpace()
    {
        while (position_ < text_.size() &&
               (text_[position_] == ' ' || text_[position_] == '\t' ||
                text_[position_] == '\r' || text_[position_] == '\n'))
        {
            ++position_;
        }
    }

    bool Consume(char expected)
    {
        SkipSpace();
        if (position_ < text_.size() && text_[position_] == expected)
        {
            ++position_;
            return true;
        }
        return false;
    }

    bool ConsumeWord(std::string_view word)
    {
        if (text_.substr(position_, word.size()) != word)
        {
            return false;
        }
        position_ += word.size();
        return true;
    }

    static void AppendUtf8(std::uint32_t code_point, std::string* out)
    {
        if (code_point < 0x80U)
        {
            out->push_back(static_cast<char>(code_point));
        }
        else if (code_point < 0x800U)
        {
            out->push_back(static_cast<char>(0xC0U | (code_point >> 6)));
            out->push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        }
        else if (code_point < 0x10000U)
        {
            out->push_back(static_cast<char>(0xE0U | (code_point >> 12)));
            out->push_back(
                static_cast<char>(0x80U | ((code_point >> 6) & 0x3FU)));
            out->push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        }
        else
        {
            out->push_back(static_cast<char>(0xF0U | (code_point >> 18)));
            out->push_back(
                static_cast<char>(0x80U | ((code_point >> 12) & 0x3FU)));
            out->push_back(
                static_cast<char>(0x80U | ((code_point >> 6) & 0x3FU)));
            out->push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        }
    }

    bool ReadHex4(std::uint32_t* value)
    {
        if (position_ + 4U > text_.size())
        {
            return false;
        }
        std::uint32_t parsed = 0;
        const auto result = std::from_chars(
            text_.data() + position_, text_.data() + position_ + 4U, parsed,
            16);
        if (result.ec != std::errc{} ||
            result.ptr != text_.data() + position_ + 4U)
        {
            return false;
        }
        position_ += 4U;
        *value = parsed;
        return true;
    }

    bool ParseString(std::string* out)
    {
        if (!Consume('"'))
        {
            return false;
        }
        while (position_ < text_.size())
        {
            const char character = text_[position_++];
            if (character == '"')
            {
                return true;
            }
            if (static_cast<unsigned char>(character) < 0x20U)
            {
                return false;
            }
            if (character != '\\')
            {
                out->push_back(character);
                continue;
            }
            if (position_ >= text_.size())
            {
                return false;
            }
            const char escape = text_[position_++];
            switch (escape)
            {
            case '"':
            case '\\':
            case '/':
                out->push_back(escape);
                break;
            case 'b':
                out->push_back('\b');
                break;
            case 'f':
                out->push_back('\f');
                break;
            case 'n':
                out->push_back('\n');
                break;
            case 'r':
                out->push_back('\r');
                break;
            case 't':
                out->push_back('\t');
                break;
            case 'u':
            {
                std::uint32_t code_point = 0;
                if (!ReadHex4(&code_point))
                {
                    return false;
                }
                if (code_point >= 0xD800U && code_point < 0xDC00U)
                {
                    std::uint32_t low = 0;
                    if (!ConsumeWord("\\u") || !ReadHex4(&low) ||
                        low < 0xDC00U || low >= 0xE000U)
                    {
                        return false;
                    }
                    code_point =
                        0x10000U + ((code_point - 0xD800U) << 10) +
                        (low - 0xDC00U);
                }
                AppendUtf8(code_point, out);
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool ParseNumber(std::string* out)
    {
        const std::size_t start = position_;
        while (position_ < text_.size())
        {
            const char character = text_[position_];
            if ((character >= '0' && character <= '9') || character == '-' ||
                character == '+' || character == '.' || character == 'e' ||
                character == 'E')
            {
                ++position_;
                continue;
            }
            break;
        }
        if (position_ == start)
        {
            return false;
        }
        out->assign(text_.substr(start, position_ - start));
        return true;
    }

    bool ParseValue(JsonValue* value, int depth)
    {
        if (depth > kMaxDepth)
        {
            return false;
        }
        SkipSpace();
        if (position_ >= text_.size())
        {
            return false;
        }
        const char character = text_[position_];
        if (character == '{')
        {
            ++position_;
            value->kind = JsonValue::Kind::kObject;
            if (Consume('}'))
            {
                return true;
            }
            do
            {
                std::pair<std::string, JsonValue> member;
                if (!ParseString(&member.first) || !Consume(':') ||
                    !ParseValue(&member.second, depth + 1))
                {
                    return false;
                }
                value->members.push_back(std::move(member));
            } while (Consume(','));
            return Consume('}');
        }
        if (character == '[')
        {
            ++position_;
            value->kind = JsonValue::Kind::kArray;
            if (Consume(']'))
            {
                return true;
            }
            do
            {
                JsonValue item;
                if (!ParseValue(&item, depth + 1))
                {
                    return false;
                }
                value->items.push_back(std::move(item));
            } while (Consume(','));
            return Consume(']');
        }
        if (character == '"')
        {
            value->kind = JsonValue::Kind::kString;
            return ParseString(&value->text);
        }
        if (ConsumeWord("true"))
        {
            value->kind = JsonValue::Kind::kBool;
            value->boolean = true;
            return true;
        }
        if (ConsumeWord("false"))
        {
            value->kind = JsonValue::Kind::kBool;
            return true;
        }
        if (ConsumeWord("null"))
        {
            value->kind = JsonValue::Kind::kNull;
            return true;
        }
        value->kind = JsonValue::Kind::kNumber;
        return ParseNumber(&value->text);
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

const std::string* StringMember(const JsonValue& object, std::string_view key)
{
    const JsonValue* member = object.Member(key);
    return member != nullptr && member->kind == JsonValue::Kind::kString
        ? &member->text
        : nullptr;
}

bool IsLowerHex(std::string_view text)
{
    for (char character : text)
    {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f')))
        {
            return false;
        }
    }
    return true;
}

}  // namespace

bool ParseLatestRelease(std::string_view json, ReleaseInfo* release,
                        std::string* error)
{
    JsonValue root;
    JsonParser parser(json);
    if (!parser.Parse(&root, error))
    {
        return false;
    }
    if (root.kind != JsonValue::Kind::kObject)
    {
        *error = "release response is not an object";
        return false;
    }
    const std::string* tag = StringMember(root, "tag_name");
    if (tag == nullptr)
    {
        // An error body ({"message": "...") lands here.
        const std::string* message = StringMember(root, "message");
        *error = message != nullptr ? "GitHub: " + *message
                                    : std::string("release has no tag_name");
        return false;
    }
    const std::optional<SemanticVersion> version = ParseSemanticVersion(*tag);
    if (!version.has_value())
    {
        *error = "release tag is not a version: " + *tag;
        return false;
    }
    ReleaseInfo parsed;
    parsed.tag = *tag;
    parsed.version = *version;
    if (const std::string* page = StringMember(root, "html_url"))
    {
        parsed.page_url = *page;
    }
    const JsonValue* assets = root.Member("assets");
    if (assets != nullptr && assets->kind == JsonValue::Kind::kArray)
    {
        for (const JsonValue& item : assets->items)
        {
            const std::string* name = StringMember(item, "name");
            const std::string* url =
                StringMember(item, "browser_download_url");
            const JsonValue* size = item.Member("size");
            if (name == nullptr || url == nullptr || size == nullptr ||
                size->kind != JsonValue::Kind::kNumber)
            {
                continue;
            }
            ReleaseAsset asset;
            asset.name = *name;
            asset.download_url = *url;
            const auto result = std::from_chars(
                size->text.data(), size->text.data() + size->text.size(),
                asset.size);
            if (result.ec != std::errc{} ||
                result.ptr != size->text.data() + size->text.size())
            {
                continue;
            }
            constexpr std::string_view kSha256Prefix = "sha256:";
            if (const std::string* digest = StringMember(item, "digest"))
            {
                const std::string_view view(*digest);
                if (view.substr(0, kSha256Prefix.size()) == kSha256Prefix &&
                    view.size() == kSha256Prefix.size() + 64U &&
                    IsLowerHex(view.substr(kSha256Prefix.size())))
                {
                    asset.sha256 = std::string(view.substr(kSha256Prefix.size()));
                }
            }
            parsed.assets.push_back(std::move(asset));
        }
    }
    *release = std::move(parsed);
    return true;
}

std::optional<std::string> ReleaseAssetName(const SemanticVersion& version,
                                            std::string_view platform,
                                            std::string_view architecture)
{
    const std::string prefix = "rePIU-v" + FormatSemanticVersion(version);
    if (platform == "Win" && architecture == "x86")
    {
        return prefix + "-win32.zip";
    }
    if (platform == "Linux" && architecture == "x64")
    {
        return prefix + "-linux-x64.tar.gz";
    }
    if (platform == "Linux" && architecture == "x86")
    {
        return prefix + "-linux-i386.tar.gz";
    }
    return std::nullopt;
}

const ReleaseAsset* FindReleaseAsset(const ReleaseInfo& release,
                                     std::string_view name)
{
    for (const ReleaseAsset& asset : release.assets)
    {
        if (asset.name == name)
        {
            return &asset;
        }
    }
    return nullptr;
}

}  // namespace repiu::update

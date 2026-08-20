#include "repiu/config/romset_config_template.h"

#include "repiu/input/host_key_names.h"

#include <algorithm>
#include <vector>

namespace repiu::config
{
namespace
{

using input::HostKeyGroup;
using input::HostKeyName;
using input::JammaInputKey;

struct GroupLabel
{
    HostKeyGroup group;
    const char* label;
};

constexpr GroupLabel kGroupLabels[] = {
    {HostKeyGroup::kLetter, "Letters"},
    {HostKeyGroup::kDigit, "Digits"},
    {HostKeyGroup::kFunction, "Function"},
    {HostKeyGroup::kKeypad, "Keypad"},
    {HostKeyGroup::kNavigation, "Navigation"},
    {HostKeyGroup::kOther, "Other"},
    {HostKeyGroup::kModifier, "Modifiers"},
};

constexpr std::size_t kCommentWidth = 78;
constexpr std::string_view kListIndent = ";               ";

class LineWriter
{
public:
    void Line(std::string_view text)
    {
        text_.append(text);
        text_.append(kRomSetConfigLineEnding);
    }

    void Blank()
    {
        Line(std::string_view());
    }

    std::string Take()
    {
        return std::move(text_);
    }

private:
    std::string text_;
};

// Emits every name of one group, wrapping so a long family such as the keypad
// stays inside a readable comment width. The whole list is generated from the
// name table, so a key added there shows up here without a second edit -- and
// the probe asserts exactly that.
void WriteGroupNames(LineWriter* writer, HostKeyGroup group,
                     std::string_view label)
{
    std::uint32_t count = 0;
    const HostKeyName* table = input::HostKeyNameTable(&count);

    std::vector<std::string_view> names;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        if (table[index].group == group)
        {
            names.push_back(table[index].name);
        }
    }
    if (names.empty())
    {
        return;
    }

    // A run of single-character names is a range, not a list: printing all 26
    // letters would bury the groups that actually need reading.
    const bool contiguous_single_characters =
        names.size() > 4 &&
        std::all_of(names.begin(), names.end(),
                    [](std::string_view name) { return name.size() == 1; });

    std::string prefix = ";   ";
    prefix.append(label);
    while (prefix.size() < kListIndent.size())
    {
        prefix.push_back(' ');
    }

    if (contiguous_single_characters)
    {
        std::string line = prefix;
        line.append(names.front());
        line.append(" .. ");
        line.append(names.back());
        writer->Line(line);
        return;
    }

    std::string line = prefix;
    bool first_on_line = true;
    for (const std::string_view name : names)
    {
        const std::size_t addition = name.size() + (first_on_line ? 0 : 2);
        if (!first_on_line && line.size() + addition > kCommentWidth)
        {
            line.push_back(',');
            writer->Line(line);
            line.assign(kListIndent);
            first_on_line = true;
        }
        if (!first_on_line)
        {
            line.append(", ");
        }
        line.append(name);
        first_on_line = false;
    }
    if (!first_on_line)
    {
        writer->Line(line);
    }
}

void WriteHeader(LineWriter* writer, std::string_view rom_set_id)
{
    std::string title = "; rePIU configuration for ROM set \"";
    title.append(rom_set_id);
    title.append("\"");
    writer->Line(title);
    writer->Line(";");
    writer->Line("; Created automatically on first run. rePIU never overwrites"
                 " an existing file,");
    writer->Line("; so anything you change here is kept.");
    writer->Line(";");
    writer->Line("; Every entry below is commented out and shows the value"
                 " currently in effect.");
    writer->Line("; Uncomment a line to override it for this ROM set only.");
    writer->Line(";");
    writer->Line("; To change a setting for every ROM set at once, put it in a"
                 " shared file");
    writer->Line("; instead: create cfg/pumpitup.ini and add the entry there."
                 " This ROM set's own");
    writer->Line("; file always wins over the shared one.");
    writer->Line(";");
    writer->Line("; Syntax");
    writer->Line(";   NAME = key            bind one host key");
    writer->Line(";   NAME = key1, key2     bind several; any one of them"
                 " works");
    writer->Line(";   NAME = Ctrl+F1        modifier combination, base key"
                 " last");
    writer->Line(";   NAME =                no value: turn this input off");
    writer->Line(";   ;  or  #              comment");
    writer->Line(";");
    writer->Line("; Modifiers");
    writer->Line(";   Ctrl, Shift, Alt        either side of the keyboard");
    writer->Line(";   LeftCtrl, RightCtrl, LeftShift, RightShift, LeftAlt,"
                 " RightAlt");
    writer->Line(";   Combine with '+': Ctrl+Shift+F2");
    writer->Line(";");
    writer->Line("; Host key names (case and underscores are ignored:"
                 " Keypad7 = KEYPAD_7)");
    for (const GroupLabel& entry : kGroupLabels)
    {
        WriteGroupNames(writer, entry.group, entry.label);
    }
    writer->Line(";");
    writer->Line("; Full reference: docs/guides/romset-config-files.md");
}

void WriteInputEntry(LineWriter* writer,
                     const input::ResolvedJammaBindings& bindings,
                     JammaInputKey key)
{
    const std::string_view name = input::JammaInputKeyConfigName(key);
    std::string line = ";";
    line.append(name);
    // Pad so the '=' columns line up across the section.
    while (line.size() < 15)
    {
        line.push_back(' ');
    }
    line.append("= ");
    line.append(input::FormatJammaBinding(bindings, key));
    writer->Line(line);
}

}  // namespace

std::string RenderRomSetConfigTemplate(
    std::string_view rom_set_id,
    const input::ResolvedJammaBindings& bindings)
{
    LineWriter writer;
    WriteHeader(&writer, rom_set_id);
    writer.Blank();

    writer.Line("[Input]");
    writer.Line("; Player 1 stage panels (I/O port 0x02A8)");
    WriteInputEntry(&writer, bindings, JammaInputKey::kP1UpLeft);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP1UpRight);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP1Center);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP1DownLeft);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP1DownRight);
    writer.Blank();

    writer.Line("; Player 2 stage panels (I/O port 0x02AA)");
    writer.Line("; Keypad names assume NumLock off; the aliases are what"
                " Windows reports then.");
    WriteInputEntry(&writer, bindings, JammaInputKey::kP2UpLeft);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP2UpRight);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP2Center);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP2DownLeft);
    WriteInputEntry(&writer, bindings, JammaInputKey::kP2DownRight);
    writer.Blank();

    writer.Line("; Cabinet buttons (I/O port 0x02A9)");
    WriteInputEntry(&writer, bindings, JammaInputKey::kTest);
    WriteInputEntry(&writer, bindings, JammaInputKey::kService);
    WriteInputEntry(&writer, bindings, JammaInputKey::kClear);
    WriteInputEntry(&writer, bindings, JammaInputKey::kCoin1);

    return writer.Take();
}

}  // namespace repiu::config

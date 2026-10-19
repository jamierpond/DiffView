#include "ReviewMarkdown.h"
#include "Text.h"

#include <algorithm>
#include <charconv>
#include <optional>

namespace diffview
{
namespace Json = Miro::Json;

namespace
{
constexpr auto separator = std::string_view {" · "};
constexpr auto gutterBar = std::string_view {"│"};
constexpr auto wholeFile = std::string_view {" (whole file)"};
constexpr auto oldSide = std::string_view {" (old)"};
constexpr auto generalLocation = std::string_view {"General"};
constexpr auto unchangedNote = std::string_view {"unchanged file"};
constexpr auto updatedFrom = std::string_view {"updated from review "};
constexpr auto replyMark = std::string_view {"#### reply"};
constexpr auto freshMark = std::string_view {"new"};

const Json::Value* field(const Json::Value& object, std::string_view key)
{
    return object.isObject() ? Json::find(object.asObject(), key) : nullptr;
}

std::string stringField(const Json::Value& object, std::string_view key)
{
    auto* value = field(object, key);
    return value != nullptr && value->isString() ? value->asString()
                                                 : std::string {};
}

std::int64_t intField(const Json::Value& object, std::string_view key)
{
    auto* value = field(object, key);
    return value != nullptr && value->isNumber() ? value->asInteger() : 0;
}

bool boolField(const Json::Value& object, std::string_view key)
{
    auto* value = field(object, key);
    return value != nullptr && value->isBool() && value->asBool();
}

std::vector<std::string> linesField(const Json::Value& object, std::string_view key)
{
    auto lines = std::vector<std::string> {};
    auto* value = field(object, key);

    if (value != nullptr && value->isArray())
        for (const auto& line: value->asArray())
            if (line.isString())
                lines.push_back(line.asString());

    return lines;
}

Json::Value toArray(const std::vector<std::string>& lines)
{
    auto array = Json::Array {};

    for (const auto& line: lines)
        array.push_back(line);

    return array;
}

bool startsWith(std::string_view text, std::string_view prefix)
{
    return text.substr(0, prefix.size()) == prefix;
}

bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size()
           && text.substr(text.size() - suffix.size()) == suffix;
}

std::string fenceMarker(std::string_view line)
{
    if (line.empty() || (line[0] != '`' && line[0] != '~'))
        return {};

    auto length = line.find_first_not_of(line[0]);
    length = length == std::string_view::npos ? line.size() : length;
    return length >= 3 ? std::string(length, line[0]) : std::string {};
}

bool closesFence(std::string_view line, const std::string& marker)
{
    auto own = fenceMarker(line);
    return !own.empty() && own[0] == marker[0] && own.size() >= marker.size()
           && trim(line.substr(own.size())).empty();
}

struct Heading
{
    std::string path;
    bool fileLevel = false;
    bool old = false;
    int start = 0;
    int end = 0;
    std::string status;
    bool stale = false;
    std::string updates;
    bool unchangedFile = false;
    std::string owner;
    int id = 0;
};

int toInt(std::string_view digits)
{
    auto value = 0;
    std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return value;
}

bool isDigits(std::string_view text)
{
    return !text.empty()
           && std::all_of(text.begin(),
                          text.end(),
                          [](char c) { return c >= '0' && c <= '9'; });
}

// "### src/a.cpp:12-14 (old) · open, stale · #4". Anything else is prose.
std::optional<Heading> parseHeading(std::string_view line)
{
    if (!startsWith(line, "### "))
        return std::nullopt;

    auto rest = trim(line.substr(4));
    auto parts = std::vector<std::string> {};
    auto from = std::size_t {0};

    while (true)
    {
        auto at = rest.find(separator, from);
        parts.push_back(rest.substr(
            from, at == std::string::npos ? std::string::npos : at - from));

        if (at == std::string::npos)
            break;

        from = at + separator.size();
    }

    auto heading = Heading {};

    if (parts.size() >= 2 && startsWith(parts.back(), "#")
        && isDigits(parts.back().substr(1)))
    {
        heading.id = toInt(std::string_view {parts.back()}.substr(1));
        parts.pop_back();
    }

    if (parts.size() >= 3 && startsWith(parts.back(), "@") && parts.back().size() > 1)
    {
        heading.owner = trim(std::string_view {parts.back()}.substr(1));
        parts.pop_back();
    }

    if (parts.size() < 2)
        return std::nullopt;

    auto status = trim(parts.back());
    auto word = status.substr(0, status.find_first_of(", "));

    if (word != "open" && word != "resolved" && word != "withdrawn")
        return std::nullopt;

    heading.status = word;
    heading.stale = status.find("stale") != std::string::npos;
    heading.unchangedFile = status.find(unchangedNote) != std::string::npos;

    if (auto at = status.find(updatedFrom); at != std::string::npos)
    {
        auto id = status.substr(at + updatedFrom.size());
        heading.updates = id.substr(0, id.find_first_of(", "));
    }
    parts.pop_back();

    // The path may itself contain " · ".
    auto location = parts.front();

    for (std::size_t i = 1; i < parts.size(); ++i)
        location += std::string {separator} + parts[i];

    auto where = std::string_view {location};

    if (where == generalLocation)
    {
        heading.fileLevel = true;
        return heading;
    }

    if (endsWith(where, wholeFile))
    {
        heading.fileLevel = true;
        heading.path =
            std::string {where.substr(0, where.size() - wholeFile.size())};
        return heading;
    }

    if (endsWith(where, oldSide))
    {
        heading.old = true;
        where.remove_suffix(oldSide.size());
    }

    auto colon = where.rfind(':');
    auto numbers = colon == std::string_view::npos ? std::string_view {}
                                                   : where.substr(colon + 1);
    auto dash = numbers.find('-');
    auto first = numbers.substr(0, dash);
    auto last = dash == std::string_view::npos ? first : numbers.substr(dash + 1);

    if (!isDigits(first) || !isDigits(last))
    {
        heading.fileLevel = true;
        heading.path = std::string {where};
        return heading;
    }

    heading.path = std::string {where.substr(0, colon)};
    heading.start = toInt(first);
    heading.end = toInt(last);
    return heading;
}

std::vector<std::string> splitParts(std::string_view text)
{
    auto parts = std::vector<std::string> {};
    auto from = std::size_t {0};

    while (true)
    {
        auto at = text.find(separator, from);
        parts.push_back(trim(text.substr(
            from, at == std::string_view::npos ? std::string_view::npos : at - from)));

        if (at == std::string_view::npos)
            return parts;

        from = at + separator.size();
    }
}

// "#### reply · diffview-dev · 2026-10-18 14:02", "· new" on the reviewer's
// first sent in this round. Author and time may be left off by hand.
std::optional<Reply> parseReplyHeading(std::string_view line)
{
    if (!startsWith(line, replyMark))
        return std::nullopt;

    auto rest = line.substr(replyMark.size());

    if (!rest.empty() && !startsWith(rest, separator) && trim(rest) != "")
        return std::nullopt;

    auto parts = splitParts(line.substr(5));
    auto reply = Reply {};

    while (parts.size() > 1 && parts.back() == freshMark)
    {
        reply.fresh = true;
        parts.pop_back();
    }

    if (parts.size() > 1)
        reply.author = parts[1];

    if (parts.size() > 2)
        reply.time = parts[2];

    return reply;
}

// A prose line that would read as a comment heading gets a backslash
// (Markdown shows "\###" as "###"); one is taken off on the way back in.
bool isSection(std::string_view line)
{
    return startsWith(line, "## ");
}

bool needsEscape(std::string_view line)
{
    auto stripped = line.substr(std::min(line.find_first_not_of('\\'), line.size()));
    return isSection(stripped) || parseHeading(stripped).has_value()
           || parseReplyHeading(stripped).has_value();
}

std::string fenceFor(const std::vector<const std::vector<std::string>*>& groups)
{
    auto longest = 0;

    for (const auto* group: groups)
        for (const auto& line: *group)
        {
            auto run = 0;

            for (auto c: line)
            {
                run = c == '`' ? run + 1 : 0;
                longest = std::max(longest, run);
            }
        }

    return std::string(static_cast<std::size_t>(std::max(3, longest + 1)), '`');
}

void writeContext(std::string& out, const Json::Value& comment)
{
    auto before = linesField(comment, "before");
    auto lines = linesField(comment, "lines");
    auto after = linesField(comment, "after");

    if (before.empty() && lines.empty() && after.empty())
        return;

    // Numbers are counted out from the target's first line.
    auto old = stringField(comment, "side") == "oldSide";
    auto skip = old ? '+' : '-';
    auto start = static_cast<int>(intField(comment, "start"));

    auto numbered = [&](const std::string& line)
    { return line.empty() || line[0] != skip; };

    auto numbers = std::vector<int> {};

    auto counting =
        static_cast<int>(std::count_if(before.begin(), before.end(), numbered));
    auto next = start - counting;

    for (const auto* group: {&before, &lines, &after})
        for (const auto& line: *group)
            numbers.push_back(numbered(line) && start > 0 ? next++ : 0);

    auto width = 2;

    for (auto number: numbers)
        width = std::max(width, static_cast<int>(std::to_string(number).size()));

    auto fence = fenceFor({&before, &lines, &after});
    out += fence + "text\n";

    auto index = std::size_t {0};

    for (const auto* group: {&before, &lines, &after})
        for (const auto& line: *group)
        {
            auto number = numbers[index++];
            auto shown = number > 0 ? std::to_string(number) : std::string {};
            out += group == &lines ? ">" : " ";
            out += " "
                   + std::string(static_cast<std::size_t>(width) - shown.size(), ' ')
                   + shown + " " + std::string {gutterBar}
                   + (line.empty() ? std::string {" "} : line) + "\n";
        }

    out += fence + "\n\n";
}

void writeProse(std::string& out, const std::string& text)
{
    auto fence = std::string {};

    for (const auto& line: splitLines(text))
    {
        if (fence.empty())
        {
            fence = fenceMarker(line);
            out += (fence.empty() && needsEscape(line) ? "\\" : "") + line + "\n";
        }
        else
        {
            if (closesFence(line, fence))
                fence.clear();

            out += line + "\n";
        }
    }

    out += "\n";
}

void writeComment(std::string& out, const Json::Value& comment)
{
    auto path = stringField(comment, "path");
    auto location = path;

    if (path.empty())
        location = std::string {generalLocation};
    else if (boolField(comment, "fileLevel"))
        location += std::string {wholeFile};
    else
    {
        auto start = intField(comment, "start");
        auto end = intField(comment, "end");
        location += ":" + std::to_string(start);

        if (end != start)
            location += "-" + std::to_string(end);

        if (stringField(comment, "side") == "oldSide")
            location += std::string {oldSide};
    }

    auto status = boolField(comment, "withdrawn")  ? std::string {"withdrawn"}
                  : boolField(comment, "resolved") ? std::string {"resolved"}
                                                   : std::string {"open"};

    if (auto updates = stringField(comment, "updates"); !updates.empty())
        status += ", " + std::string {updatedFrom} + updates;

    if (boolField(comment, "unchangedFile"))
        status += ", " + std::string {unchangedNote};

    if (boolField(comment, "stale"))
        status += ", stale: its lines have left the diff";

    out += "### " + location + std::string {separator} + status;

    if (auto owner = stringField(comment, "owner"); !owner.empty())
        out += std::string {separator} + "@" + owner;

    if (auto id = intField(comment, "id"); id > 0)
        out += std::string {separator} + "#" + std::to_string(id);

    out += "\n\n";

    if (!boolField(comment, "fileLevel"))
        writeContext(out, comment);

    writeProse(out, stringField(comment, "text"));

    if (auto* replies = field(comment, "replies");
        replies != nullptr && replies->isArray())
        for (const auto& reply: replies->asArray())
        {
            out += std::string {replyMark};

            for (const auto* key: {"author", "time"})
                if (auto value = stringField(reply, key); !value.empty())
                    out += std::string {separator} + value;

            if (boolField(reply, "fresh"))
                out += std::string {separator} + std::string {freshMark};

            out += "\n\n";
            writeProse(out, stringField(reply, "text"));
        }
}

std::optional<std::pair<bool, std::string>> parseContextLine(std::string_view line)
{
    auto bar = line.find(gutterBar);

    if (bar == std::string_view::npos)
        return std::nullopt;

    auto left = line.substr(0, bar);
    auto right = std::string {line.substr(bar + gutterBar.size())};

    if (right.empty())
        right = " "; // an unchanged empty line whose trailing space was trimmed

    return std::make_pair(trim(left).starts_with(">"), right);
}

struct Block
{
    Heading heading;
    std::vector<std::string> body;
};

Json::Value commentValue(const Block& block)
{
    auto comment = Json::Object {};
    const auto& heading = block.heading;

    comment["id"] = heading.id;
    comment["path"] = heading.path;
    comment["fileLevel"] = heading.fileLevel;
    comment["side"] = heading.old ? "oldSide" : "newSide";
    comment["start"] = heading.start;
    comment["end"] = heading.end;
    comment["resolved"] = heading.status == "resolved";
    comment["withdrawn"] = heading.status == "withdrawn";
    comment["stale"] = heading.stale;
    comment["updates"] = heading.updates;
    comment["unchangedFile"] = heading.unchangedFile;
    comment["owner"] = heading.owner;

    auto before = std::vector<std::string> {};
    auto lines = std::vector<std::string> {};
    auto after = std::vector<std::string> {};

    auto body = block.body;
    auto i = std::size_t {0};

    while (i < body.size() && trim(body[i]).empty())
        ++i;

    // Only a fence whose every line is a "> 46 │+text" row is the context
    // block; otherwise the comment opens with its own code block.
    if (!heading.fileLevel && i < body.size())
        if (auto fence = fenceMarker(body[i]); !fence.empty())
        {
            auto close = i + 1;

            while (close < body.size() && !closesFence(body[close], fence))
                ++close;

            auto rows = std::vector<std::pair<bool, std::string>> {};
            auto isContext = close < body.size();

            for (auto k = i + 1; k < close && isContext; ++k)
            {
                if (trim(body[k]).empty())
                    continue;

                auto row = parseContextLine(body[k]);
                isContext = row.has_value();

                if (row)
                    rows.push_back(*row);
            }

            if (isContext)
            {
                auto seenTarget = false;

                for (const auto& [target, text]: rows)
                {
                    if (target)
                    {
                        lines.push_back(text);
                        seenTarget = true;
                    }
                    else
                        (seenTarget ? after : before).push_back(text);
                }

                // No marked lines: the whole block is what it is about.
                if (lines.empty())
                {
                    lines = std::move(before);
                    before.clear();
                }

                i = close + 1;
            }
        }

    comment["before"] = toArray(before);
    comment["lines"] = toArray(lines);
    comment["after"] = toArray(after);

    // The comment, then each reply: prose split at reply headings outside
    // fences.
    auto segments = std::vector<std::vector<std::string>> {{}};
    auto replies = Json::Array {};
    auto fence = std::string {};

    for (; i < body.size(); ++i)
    {
        auto line = body[i];

        if (fence.empty())
        {
            if (auto reply = parseReplyHeading(line))
            {
                auto value = Json::Object {};
                value["author"] = reply->author;
                value["time"] = reply->time;
                value["fresh"] = reply->fresh;
                replies.push_back(std::move(value));
                segments.emplace_back();
                continue;
            }

            fence = fenceMarker(line);

            if (fence.empty() && line.starts_with("\\")
                && needsEscape(line.substr(1)))
                line = line.substr(1);
        }
        else if (closesFence(line, fence))
            fence.clear();

        segments.back().push_back(line);
    }

    auto joined = [](std::vector<std::string> text)
    {
        while (!text.empty() && trim(text.front()).empty())
            text.erase(text.begin());

        while (!text.empty() && trim(text.back()).empty())
            text.pop_back();

        auto result = std::string {};

        for (std::size_t k = 0; k < text.size(); ++k)
            result += (k == 0 ? "" : "\n") + text[k];

        return result;
    };

    comment["text"] = joined(segments.front());

    for (std::size_t k = 0; k < replies.size(); ++k)
        replies[k].asObject()["text"] = joined(segments[k + 1]);

    comment["replies"] = std::move(replies);
    return comment;
}
} // namespace

std::string printMarkdown(const Miro::JSON& review)
{
    auto title = stringField(review, "title");
    auto baseLabel = stringField(review, "baseLabel");
    auto baseSha = stringField(review, "baseSha");

    auto out = std::string {};

    auto against = "diffed against `" + baseLabel + "`"
                   + (baseSha.empty() ? "" : " at " + baseSha);

    if (title == "draft" || title.empty())
    {
        // For the person writing it, not an agent: it hasn't been sent.
        out += "# DiffView draft · not sent yet\n\n";
        out +=
            "Your comments on the changes " + against
            + "; edit freely, DiffView reloads this file, and S sends it as a round.\n\n";
    }
    else
    {
        out += "# DiffView review · " + title + "\n\n";
        out += "Review comments on this repo's changes, " + against + ".\n";
        out += "Each `### path:lines · status · #id` heading is one comment: "
               "working-tree line numbers, `(old)` the base's, `(whole file)` the "
               "whole file.\n";
        out +=
            "In the block under it `>` marks the lines the comment is about (line "
            "number, then `+` added, `-` removed, space unchanged); the comment "
            "follows the block.\n";
        out += "Replies follow a comment as `#### reply · author · time`: `you` is "
               "the reviewer, `new` marks theirs new in this round. `@name` after "
               "the status is the agent the thread belongs to.\n";
        out +=
            "Act on `open` comments; `resolved` and `withdrawn` ones need nothing. "
            "Answer with `dv reply <id> \"text\"`; when you have dealt with one, "
            "`dv resolve <id>` (or change its `· open` to `· resolved` here).\n\n";
    }

    auto section = std::optional<std::string> {};

    if (auto* comments = field(review, "comments");
        comments != nullptr && comments->isArray())
        for (const auto& comment: comments->asArray())
        {
            auto path = stringField(comment, "path");

            if (section != path)
            {
                out += "## " + (path.empty() ? std::string {generalLocation} : path)
                       + "\n\n";
                section = path;
            }

            writeComment(out, comment);
        }

    while (out.size() >= 2 && out[out.size() - 1] == '\n'
           && out[out.size() - 2] == '\n')
        out.pop_back();

    return out;
}

Miro::JSON parseMarkdown(std::string_view input)
{
    auto review = Json::Object {};
    auto comments = Json::Array {};
    review["title"] = "draft";
    review["baseLabel"] = "";
    review["baseSha"] = "";

    auto lines = splitLines(input);

    for (auto& line: lines)
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

    auto blocks = std::vector<Block> {};
    auto fence = std::string {};

    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const auto& line = lines[index];

        // A heading can't start inside a fence: a comment may quote one.
        if (fence.empty())
        {
            if (auto heading = parseHeading(line))
            {
                blocks.push_back({*heading, {}});
                continue;
            }

            // A "## path" line straight above a comment heading is one of our
            // section lines; anywhere else it is prose.
            if (isSection(line))
            {
                auto next = index + 1;

                while (next < lines.size() && trim(lines[next]).empty())
                    ++next;

                if (next < lines.size() && parseHeading(lines[next]))
                    continue;
            }

            if (blocks.empty())
            {
                if (line.starts_with("# DiffView review · "))
                    review["title"] = trim(std::string_view {line}.substr(
                        std::string_view {"# DiffView review · "}.size()));

                if (auto open = line.find("diffed against `");
                    open != std::string::npos)
                {
                    auto start = open + std::string_view {"diffed against `"}.size();
                    auto close = line.find('`', start);

                    if (close != std::string::npos)
                    {
                        review["baseLabel"] = line.substr(start, close - start);
                        // " at <sha>" straight after the label.
                        if (line.compare(close + 1, 4, " at ") == 0)
                        {
                            auto sha = close + 5;
                            auto end =
                                line.find_first_not_of("0123456789abcdef", sha);
                            review["baseSha"] = line.substr(sha, end - sha);
                        }
                    }
                }

                continue;
            }

            fence = fenceMarker(line);
        }
        else if (closesFence(line, fence))
        {
            fence.clear();
        }

        if (!blocks.empty())
            blocks.back().body.push_back(line);
    }

    for (const auto& block: blocks)
        comments.push_back(commentValue(block));

    review["comments"] = std::move(comments);
    return review;
}

std::string commentMarkdown(const Annotation& comment)
{
    auto out = std::string {};
    writeComment(out, Miro::toJSON(comment));

    while (out.size() >= 2 && out[out.size() - 1] == '\n'
           && out[out.size() - 2] == '\n')
        out.pop_back();

    return out;
}

std::string toMarkdown(const ReviewFile& review)
{
    return printMarkdown(Miro::toJSON(review));
}

ReviewFile fromMarkdown(std::string_view text)
{
    auto review = ReviewFile {};
    Miro::fromJSON(review, parseMarkdown(text));
    return review;
}
} // namespace diffview

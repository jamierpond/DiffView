#include "Commands.h"
#include "Viewer.h"

#include <Review.h>
#include <Text.h>

#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace diffview
{
namespace
{
namespace fs = std::filesystem;

std::string readFile(const fs::path& path)
{
    auto in = std::ifstream {path, std::ios::binary};
    auto buffer = std::stringstream {};
    buffer << in.rdbuf();
    return buffer.str();
}

void writeFile(const fs::path& path, const std::string& text)
{
    auto bytes =
        std::span {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
    eacp::Files::writeFileAtomically(path.string(), bytes);
}

std::vector<ReviewFile> loadRounds(const std::string& repo)
{
    auto dir = fs::path {repo} / reviewDirectory;
    auto names = std::vector<fs::path> {};
    auto error = std::error_code {};

    if (fs::is_directory(dir, error))
        for (const auto& entry: fs::directory_iterator {dir, error})
            if (entry.path().extension() == ".md")
                names.push_back(entry.path());

    std::sort(names.begin(), names.end());

    auto rounds = std::vector<ReviewFile> {};

    for (const auto& path: names)
    {
        rounds.push_back(fromMarkdown(readFile(path)));
        rounds.back().title = path.stem().string();
    }

    return rounds;
}

bool contains(const ReviewFile& file, int id)
{
    return std::any_of(file.comments.begin(),
                       file.comments.end(),
                       [&](const Annotation& a) { return a.id == id; });
}

int fail(const std::string& message)
{
    std::fprintf(stderr, "dv: %s\n", message.c_str());
    return 1;
}

std::string describeMissing(int id)
{
    return "no comment #" + std::to_string(id)
           + " in a submitted round (dv comments lists them)";
}

class RepoLock
{
public:
    explicit RepoLock(const std::string& repo)
    {
        auto path = fs::path {repo} / annotationDirectory / ".lock";
        fd = open(path.c_str(), O_CREAT | O_RDWR, 0644);

        if (fd >= 0)
            flock(fd, LOCK_EX);
    }

    ~RepoLock()
    {
        if (fd >= 0)
            close(fd);
    }

    RepoLock(const RepoLock&) = delete;
    RepoLock& operator=(const RepoLock&) = delete;

private:
    int fd = -1;
};

int changeAndWrite(const ThreadOptions& options, bool resolve)
{
    auto root = fs::path {options.repo};

    if (!fs::is_directory(root / reviewDirectory))
        return fail("no review rounds in " + options.repo
                    + " yet (nothing has been submitted)");

    auto lock = RepoLock {options.repo};
    auto rounds = loadRounds(options.repo);
    auto at =
        std::find_if(rounds.rbegin(),
                     rounds.rend(),
                     [&](const ReviewFile& r) { return contains(r, options.id); });

    if (at == rounds.rend())
    {
        auto draftPath = root / draftFileName;

        if (fs::exists(draftPath)
            && contains(fromMarkdown(readFile(draftPath)), options.id))
            return fail("#" + std::to_string(options.id)
                        + " is still in the reviewer's draft, not sent to you");

        return fail(describeMissing(options.id));
    }

    auto& round = *at;
    auto isNewest = at == rounds.rbegin();
    auto latestPath = root / latestReviewFileName;

    if (isNewest && fs::exists(latestPath))
        if (auto synced = resolutionsFrom(fromMarkdown(readFile(latestPath)), round))
            round = std::move(*synced);

    auto change = ThreadChange {};
    change.from = options.agent.empty() ? "agent" : options.agent;
    change.named = !options.agent.empty();
    change.force = options.force;
    change.resolve = resolve;
    change.text = options.text;
    change.time = formatReplyTime(nowMs());

    if (auto error = changeThread(round, options.id, change))
        return fail(*error);

    auto text = toMarkdown(round);
    auto file = fs::path {reviewDirectory} / (round.title + ".md");
    writeFile(root / file, text);

    if (isNewest)
        writeFile(latestPath, text);

    std::printf("dv: %s #%d · %s\n",
                resolve ? "resolved" : "replied to",
                options.id,
                (root / file).string().c_str());
    return 0;
}

std::string formatLocation(const Annotation& comment)
{
    if (comment.path.empty())
        return "General";

    if (comment.fileLevel)
        return comment.path + " (whole file)";

    return comment.path + ":" + lineLabel(comment);
}

std::string pad(const std::string& text, int width)
{
    auto shown = codepointCount(text);
    return shown < width
               ? text + std::string(static_cast<std::size_t>(width - shown), ' ')
               : text;
}

const Annotation* find(const std::vector<Annotation>& comments, int id)
{
    for (const auto& comment: comments)
        if (comment.id == id)
            return &comment;

    return nullptr;
}
} // namespace

int runReply(const ThreadOptions& options)
{
    if (trim(options.text).empty())
        return fail("nothing to say: give the text, -m <text>, or pipe it in");

    return changeAndWrite(options, false);
}

int runResolve(const ThreadOptions& options)
{
    return changeAndWrite(options, true);
}

int runComments(const ListOptions& options)
{
    auto comments = mergeReviews(loadRounds(options.repo), {}).annotations;
    auto rows = std::vector<std::array<std::string, 5>> {};

    for (const auto& comment: comments)
    {
        auto owner = findOwner(comment);

        if (options.openOnly && comment.resolved)
            continue;

        if (!options.agent.empty() && !owner.empty() && owner != options.agent)
            continue;

        auto lines = splitLines(comment.text);
        auto pieces = wrap(lines.empty() ? std::string {} : lines.front(), 72);
        auto about = trim(pieces.front())
                     + (pieces.size() > 1 || lines.size() > 1 ? "…" : "");

        if (!comment.replies.empty())
            about += " · " + std::to_string(comment.replies.size())
                     + (comment.replies.size() == 1 ? " reply" : " replies")
                     + ", last " + comment.replies.back().author;

        rows.push_back({"#" + std::to_string(comment.id),
                        comment.resolved ? "resolved" : "open",
                        owner.empty() ? "-" : "@" + owner,
                        formatLocation(comment),
                        about});
    }

    auto widths = std::array<int, 4> {};

    for (const auto& row: rows)
        for (std::size_t k = 0; k < widths.size(); ++k)
            widths[k] = std::max(widths[k], codepointCount(row[k]));

    for (const auto& row: rows)
    {
        auto line = std::string {};

        for (std::size_t k = 0; k < widths.size(); ++k)
            line += pad(row[k], widths[k]) + "  ";

        std::printf("%s\n", (line + row[4]).c_str());
    }

    return 0;
}

int runShow(const std::string& repo, int id)
{
    auto comments = mergeReviews(loadRounds(repo), {}).annotations;
    const auto* comment = find(comments, id);

    if (comment == nullptr)
        return fail(describeMissing(id));

    auto shown = *comment;
    shown.owner = findOwner(shown);
    shown.updates.clear();
    std::printf("%s", printComment(shown).c_str());
    std::printf("(round %s)\n", comment->round.c_str());
    return 0;
}
} // namespace diffview

#include "Store.h"
#include "Git.h"
#include "Text.h"

#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace diffview
{
namespace
{
namespace fs = std::filesystem;

constexpr auto gitFileName = ".git";
constexpr auto gitDirectoryPrefix = std::string_view {"gitdir: "};
constexpr auto lockFileName = ".lock";
constexpr auto ignoreFileName = ".gitignore";
constexpr auto imageLinkOpening = std::string_view {"]("};

std::string makeCanonical(const std::string& path)
{
    auto error = std::error_code {};
    auto canonical = fs::weakly_canonical(fs::path {path}, error);

    if (error)
        return path;

    auto text = canonical.string();

    while (text.size() > 1 && text.back() == '/')
        text.pop_back();

    return text;
}

std::vector<ReviewFile> readRoundsIn(const eacp::FilePath& directory)
{
    auto rounds = std::vector<ReviewFile> {};

    for (const auto& id: listRoundIdsInOrder(directory))
    {
        rounds.push_back(
            readReviewOrEmpty(directory / reviewDirectory / (id + ".md")));
        rounds.back().title = id;
    }

    return rounds;
}

eacp::FilePath getRoundPath(const eacp::FilePath& directory, const std::string& id)
{
    return directory / reviewDirectory / (id + ".md");
}

// An agent resolves comments in latest.md; they belong in the round's file
// before that copy is replaced.
void syncLatestIntoItsRound(const eacp::FilePath& directory,
                            std::vector<ReviewFile>& rounds)
{
    auto latest = readReviewOrEmpty(directory / latestReviewFileName);

    for (auto& round: rounds)
    {
        if (round.title != latest.title)
            continue;

        if (auto resolved = resolutionsFrom(latest, round))
            round = std::move(*resolved);
    }
}

void syncLatestIntoItsRoundFile(const eacp::FilePath& directory)
{
    auto latest = readReviewOrEmpty(directory / latestReviewFileName);
    auto path = getRoundPath(directory, latest.title);

    if (!eacp::Files::exists(path))
        return;

    auto round = readReviewOrEmpty(path);
    round.title = latest.title;

    if (auto resolved = resolutionsFrom(latest, round))
        writeText(path, toMarkdown(*resolved));
}

void writeNewestFullRoundAsLatest(const eacp::FilePath& directory)
{
    auto ids = listRoundIdsInOrder(directory);

    for (auto id = ids.rbegin(); id != ids.rend(); ++id)
    {
        auto path = getRoundPath(directory, *id);

        if (readReviewOrEmpty(path).reply)
            continue;

        auto latestPath = directory / latestReviewFileName;

        if (readReviewOrEmpty(latestPath).title != *id)
            writeText(latestPath, eacp::Files::readFile(path));

        return;
    }
}

std::string readWorktreeAdminName(const eacp::FilePath& worktree)
{
    for (const auto& line: splitLines(eacp::Files::readFile(worktree / gitFileName)))
    {
        if (line.starts_with(gitDirectoryPrefix))
            return getWorktreeShortName(
                trim(line.substr(gitDirectoryPrefix.size())));
    }

    return getWorktreeShortName(worktree.str());
}

void collectIds(const ReviewFile& file, std::set<int>& ids)
{
    for (const auto& comment: file.comments)
    {
        if (comment.id > 0)
            ids.insert(comment.id);
    }
}

std::map<int, int> calculateIdRenumbering(const std::set<int>& own,
                                          const std::set<int>& taken)
{
    auto renumbering = std::map<int, int> {};
    auto next = 1;

    for (const auto* ids: {&own, &taken})
    {
        if (!ids->empty())
            next = std::max(next, *ids->rbegin() + 1);
    }

    for (auto id: own)
    {
        if (taken.contains(id))
            renumbering[id] = next++;
    }

    return renumbering;
}

std::map<std::string, std::string>
    calculateRoundRenaming(const std::vector<ReviewFile>& rounds,
                           const std::set<std::string>& taken)
{
    auto renaming = std::map<std::string, std::string> {};
    auto used = taken;

    for (const auto& round: rounds)
        used.insert(round.title);

    for (const auto& round: rounds)
    {
        if (!taken.contains(round.title))
            continue;

        auto renamed = round.title;

        for (auto n = 2; used.contains(renamed); ++n)
            renamed = round.title + "-" + std::to_string(n);

        used.insert(renamed);
        renaming[round.title] = renamed;
    }

    return renaming;
}

void replaceAll(std::string& text, const std::string& from, const std::string& to)
{
    for (auto at = text.find(from); at != std::string::npos;
         at = text.find(from, at + to.size()))
    {
        text.replace(at, from.size(), to);
    }
}

void relinkImages(ReviewFile& file, const std::map<std::string, std::string>& links)
{
    auto relink = [&](std::string& text)
    {
        for (const auto& [from, to]: links)
        {
            replaceAll(text,
                       std::string {imageLinkOpening} + from + ")",
                       std::string {imageLinkOpening} + to + ")");
        }
    };

    for (auto& comment: file.comments)
    {
        relink(comment.text);

        for (auto& reply: comment.replies)
            relink(reply.text);
    }
}

void renumberFile(ReviewFile& file,
                  const std::map<int, int>& renumbering,
                  const std::map<std::string, std::string>& renaming)
{
    auto notes = std::vector<std::string> {};

    if (auto renamed = renaming.find(file.title); renamed != renaming.end())
    {
        notes.push_back("was round " + file.title);
        file.title = renamed->second;
    }

    for (auto& comment: file.comments)
    {
        if (auto renamed = renaming.find(comment.updates); renamed != renaming.end())
            comment.updates = renamed->second;

        auto renumbered = renumbering.find(comment.id);

        if (renumbered == renumbering.end())
            continue;

        notes.push_back("#" + std::to_string(comment.id) + " is now #"
                        + std::to_string(renumbered->second));
        comment.id = renumbered->second;
    }

    if (notes.empty())
        return;

    file.renumbering.clear();

    for (const auto& note: notes)
        file.renumbering += (file.renumbering.empty() ? "" : ", ") + note;
}

std::optional<int> parseImageId(const std::string& fileName)
{
    auto dash = fileName.find('-');

    if (dash == std::string::npos || dash == 0)
        return std::nullopt;

    auto digits = fileName.substr(0, dash);

    if (digits.find_first_not_of("0123456789") != std::string::npos)
        return std::nullopt;

    return std::stoi(digits);
}

std::string chooseFreeImageName(const eacp::FilePath& directory,
                                const std::string& fileName,
                                const std::map<int, int>& renumbering)
{
    auto id = parseImageId(fileName);

    if (!id)
        return eacp::Files::nonexistentChild(
                   directory, eacp::FilePath {fileName}.stem(), ".png")
            .fileName();

    auto target = *id;

    if (auto renumbered = renumbering.find(*id); renumbered != renumbering.end())
        target = renumbered->second;

    auto rest = fileName.substr(fileName.find('-'));
    auto name = std::to_string(target) + rest;

    for (auto n = 2; eacp::Files::exists(directory / name); ++n)
        name = std::to_string(target) + "-merged" + std::to_string(n) + ".png";

    return name;
}

std::map<std::string, std::string> moveImages(const eacp::FilePath& from,
                                              const eacp::FilePath& home,
                                              const std::map<int, int>& renumbering)
{
    auto links = std::map<std::string, std::string> {};
    auto to = home / imageDirectory;

    for (const auto& path: eacp::Files::listFiles(from))
    {
        auto name = chooseFreeImageName(to, path.fileName(), renumbering);
        eacp::Files::moveFile(path, to / name);

        if (name != path.fileName())
        {
            links[std::string {imageDirectory} + "/" + path.fileName()] =
                std::string {imageDirectory} + "/" + name;
        }
    }

    return links;
}

std::string renumberSeen(const std::string& seen,
                         const std::map<int, int>& renumbering)
{
    auto out = std::string {};

    for (const auto& line: splitLines(seen))
    {
        auto space = line.find(' ');
        auto id = parseImageId(line.substr(0, space) + "-");

        if (!id || space == std::string::npos || !renumbering.contains(*id))
        {
            out += line + "\n";
            continue;
        }

        out += std::to_string(renumbering.at(*id)) + line.substr(space) + "\n";
    }

    return out;
}

bool isKnownStoreEntry(const std::string& name)
{
    static const auto known = std::set<std::string> {ignoreFileName,
                                                     lockFileName,
                                                     draftFileName,
                                                     latestReviewFileName,
                                                     seenFileName,
                                                     reviewDirectory,
                                                     "images",
                                                     agentDirectory};
    return known.contains(name);
}

std::string describeMerge(const eacp::FilePath& old,
                          const eacp::FilePath& store,
                          std::size_t roundCount,
                          const std::vector<ReviewFile>& rounds)
{
    auto line = "merged " + old.str() + " into " + store.str() + " ("
                + std::to_string(roundCount)
                + (roundCount == 1 ? " round)" : " rounds)");

    for (const auto& round: rounds)
    {
        if (!round.renumbering.empty())
            line += "; round " + round.title + ": " + round.renumbering;
    }

    return line;
}

std::string mergeWorktreeStore(const StoreLayout& store,
                               const eacp::FilePath& worktree)
{
    auto old = worktree / storeDirectoryName;
    auto rounds = readRoundsIn(old);
    syncLatestIntoItsRound(old, rounds);

    auto draftPath = old / draftFileName;
    auto hasDraft = eacp::Files::exists(draftPath);
    auto draft = readReviewOrEmpty(draftPath);

    for (auto& round: rounds)
    {
        if (round.worktree.empty())
            round.worktree = worktree.str();
    }

    auto taken = std::set<int> {};

    for (const auto& round: readRoundsIn(store.directory))
        collectIds(round, taken);

    for (const auto& path: listDraftFiles(store))
        collectIds(readReviewOrEmpty(path), taken);

    auto own = std::set<int> {};

    for (const auto& round: rounds)
        collectIds(round, own);

    collectIds(draft, own);

    auto takenRounds = std::set<std::string> {};

    for (const auto& id: listRoundIdsInOrder(store.directory))
        takenRounds.insert(id);

    auto renumbering = calculateIdRenumbering(own, taken);
    auto renaming = calculateRoundRenaming(rounds, takenRounds);
    auto links = moveImages(old / "images", store.home, renumbering);

    for (auto& round: rounds)
    {
        renumberFile(round, renumbering, renaming);
        relinkImages(round, links);
        writeText(getRoundPath(store.directory, round.title), toMarkdown(round));
    }

    auto ownDirectory =
        store.directory / worktreeDirectoryName / readWorktreeAdminName(worktree);

    if (hasDraft)
    {
        renumberFile(draft, renumbering, renaming);
        draft.renumbering.clear();
        relinkImages(draft, links);

        auto merged = readReviewOrEmpty(ownDirectory / draftFileName);
        merged.baseLabel = draft.baseLabel;
        merged.baseSha = draft.baseSha;
        merged.comments.insert(
            merged.comments.end(), draft.comments.begin(), draft.comments.end());
        writeText(ownDirectory / draftFileName, toMarkdown(merged));
    }

    auto seen = eacp::Files::readFile(old / seenFileName);

    if (!seen.empty())
        eacp::Files::appendFile(ownDirectory / seenFileName,
                                renumberSeen(seen, renumbering));

    for (const auto& path: eacp::Files::listFiles(old / agentDirectory))
    {
        auto target = store.directory / agentDirectory / path.fileName();

        if (!eacp::Files::exists(target))
            eacp::Files::moveFile(path, target);
    }

    for (const auto& entry: eacp::Files::listDirectory(old, {.includeHidden = true}))
    {
        auto name = entry.path.fileName();

        if (!isKnownStoreEntry(name))
            eacp::Files::moveFile(entry.path, ownDirectory / name);
    }

    eacp::Files::removeAll(old);
    return describeMerge(old, store.directory, rounds.size(), rounds);
}

std::vector<eacp::FilePath> findUnmergedWorktreeStores(const StoreLayout& store)
{
    auto pending = std::vector<eacp::FilePath> {};

    for (const auto& path: store.linkedWorktrees)
    {
        auto old = eacp::FilePath {path} / storeDirectoryName;

        if (eacp::Files::isDirectory(old)
            && !isSamePath(old.str(), store.directory.str()))
            pending.emplace_back(path);
    }

    return pending;
}
} // namespace

std::vector<WorktreeEntry> parseWorktreeList(const std::string& porcelain)
{
    constexpr auto worktreePrefix = std::string_view {"worktree "};
    auto entries = std::vector<WorktreeEntry> {};

    for (const auto& line: splitLines(porcelain))
    {
        if (line.starts_with(worktreePrefix))
            entries.push_back({line.substr(worktreePrefix.size()), false});
        else if (line == "bare" && !entries.empty())
            entries.back().bare = true;
    }

    return entries;
}

StoreLayout calculateStoreLayout(const std::string& toplevel,
                                 const std::string& gitDirectory,
                                 const std::string& commonDirectory,
                                 const std::vector<WorktreeEntry>& worktrees)
{
    auto layout = StoreLayout {};
    layout.worktree = makeCanonical(toplevel);

    auto home = [&]
    {
        if (worktrees.empty())
            return makeCanonical(toplevel);

        if (worktrees.front().bare)
            return makeCanonical(commonDirectory);

        return makeCanonical(worktrees.front().path);
    }();

    layout.home = home;
    layout.directory = layout.home / storeDirectoryName;

    if (isSamePath(gitDirectory, commonDirectory))
        layout.worktreeDirectory = layout.directory;
    else
    {
        layout.worktreeName = getWorktreeShortName(makeCanonical(gitDirectory));
        layout.worktreeDirectory =
            layout.directory / worktreeDirectoryName / layout.worktreeName;
    }

    for (const auto& entry: worktrees)
    {
        if (!entry.bare && !isSamePath(entry.path, home))
            layout.linkedWorktrees.push_back(makeCanonical(entry.path));
    }

    return layout;
}

std::optional<StoreLayout> findStore(const std::string& worktree)
{
    auto paths = git::run(git::command(worktree,
                                       {"rev-parse",
                                        "--path-format=absolute",
                                        "--show-toplevel",
                                        "--git-dir",
                                        "--git-common-dir"}));

    if (!paths)
        return std::nullopt;

    auto lines = splitLines(trim(*paths));
    constexpr auto pathCount = 3;

    if (lines.size() != pathCount)
        return std::nullopt;

    auto list =
        git::run(git::command(worktree, {"worktree", "list", "--porcelain"}));

    return calculateStoreLayout(
        lines[0], lines[1], lines[2], parseWorktreeList(list.value_or("")));
}

std::string getWorktreeShortName(const std::string& worktreePath)
{
    auto path = fs::path {worktreePath};

    if (!path.has_filename())
        path = path.parent_path();

    return path.filename().string();
}

bool isSamePath(const std::string& a, const std::string& b)
{
    return makeCanonical(a) == makeCanonical(b);
}

void createStoreDirectory(const eacp::FilePath& directory)
{
    eacp::Files::createDirectories(directory);

    // Ignores itself, so an agent's `git add -A` never sweeps it up.
    if (!eacp::Files::exists(directory / ignoreFileName))
        writeText(directory / ignoreFileName, "*\n");
}

std::vector<std::string> listRoundIdsInOrder(const eacp::FilePath& store)
{
    auto ids = std::vector<std::string> {};

    // Rounds are written atomically, so a name that is there is a whole file.
    for (const auto& path:
         eacp::Files::listFiles(store / reviewDirectory, {.pattern = "*.md"}))
    {
        ids.push_back(path.stem());
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<ReviewFile> readRounds(const StoreLayout& store)
{
    auto rounds = readRoundsIn(store.directory);

    for (auto& round: rounds)
    {
        if (round.worktree.empty())
            round.worktree = store.home.str();
    }

    return rounds;
}

ReviewFile readReviewOrEmpty(const eacp::FilePath& path)
{
    return fromMarkdown(eacp::Files::readFile(path));
}

void writeText(const eacp::FilePath& path, std::string_view text)
{
    auto bytes =
        std::span {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
    eacp::Files::writeFileAtomically(path, bytes);
}

std::string readFileFrom(const eacp::FilePath& path, std::uint64_t offset)
{
    auto in = std::ifstream {path.str(), std::ios::binary};
    in.seekg(static_cast<std::streamoff>(offset));

    if (!in)
        return {};

    return {std::istreambuf_iterator<char> {in}, std::istreambuf_iterator<char> {}};
}

std::vector<eacp::FilePath> listDraftFiles(const StoreLayout& store)
{
    auto drafts = std::vector<eacp::FilePath> {};

    if (eacp::Files::exists(store.directory / draftFileName))
        drafts.push_back(store.directory / draftFileName);

    for (const auto& entry:
         eacp::Files::listDirectory(store.directory / worktreeDirectoryName))
    {
        auto draft = entry.path / draftFileName;

        if (eacp::Files::exists(draft))
            drafts.push_back(draft);
    }

    return drafts;
}

int findHighestIdInOtherDrafts(const StoreLayout& store)
{
    auto ids = std::set<int> {};

    for (const auto& path: listDraftFiles(store))
    {
        if (path != store.worktreeDirectory / draftFileName)
            collectIds(readReviewOrEmpty(path), ids);
    }

    return ids.empty() ? 0 : *ids.rbegin();
}

StoreLock::StoreLock(const eacp::FilePath& directory)
{
    auto path = directory / lockFileName;
    descriptor = open(path.c_str(), O_CREAT | O_RDWR, 0644);

    if (descriptor >= 0)
        flock(descriptor, LOCK_EX);
}

StoreLock::~StoreLock()
{
    if (descriptor >= 0)
        close(descriptor);
}

std::vector<std::string> mergeWorktreeStores(const StoreLayout& store)
{
    if (findUnmergedWorktreeStores(store).empty())
        return {};

    createStoreDirectory(store.directory);
    auto lock = StoreLock {store.directory};

    syncLatestIntoItsRoundFile(store.directory);

    auto notes = std::vector<std::string> {};

    for (const auto& worktree: findUnmergedWorktreeStores(store))
        notes.push_back(mergeWorktreeStore(store, worktree));

    writeNewestFullRoundAsLatest(store.directory);
    return notes;
}
} // namespace diffview

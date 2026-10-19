#pragma once

#include <Review.h>

#include <eacp/Core/Utils/FilePath.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace diffview
{
struct WorktreeEntry
{
    std::string path;
    bool bare = false;
};

// As git does worktrees: one store beside the main worktree's .git, and a
// linked worktree's own draft and seen state in worktrees/<name>, as its HEAD
// is in .git/worktrees/<name>; the main worktree's are in the store itself.
struct StoreLayout
{
    eacp::FilePath home;
    eacp::FilePath directory;
    eacp::FilePath worktree;
    std::string worktreeName;
    eacp::FilePath worktreeDirectory;
    std::vector<std::string> linkedWorktrees;
};

std::vector<WorktreeEntry> parseWorktreeList(const std::string& porcelain);

StoreLayout calculateStoreLayout(const std::string& toplevel,
                                 const std::string& gitDirectory,
                                 const std::string& commonDirectory,
                                 const std::vector<WorktreeEntry>& worktrees);

std::optional<StoreLayout> findStore(const std::string& worktree);

std::string getWorktreeShortName(const std::string& worktreePath);

bool isSamePath(const std::string& a, const std::string& b);

void createStoreDirectory(const eacp::FilePath& directory);

std::vector<std::string> listRoundIdsInOrder(const eacp::FilePath& store);

// A round with no worktree line predates worktree tagging: it was reviewed in
// the main worktree, so it is read as `home`'s.
std::vector<ReviewFile> readRounds(const StoreLayout& store);

ReviewFile readReviewOrEmpty(const eacp::FilePath& path);

void writeText(const eacp::FilePath& path, std::string_view text);

// Writes pasted PNG bytes into home's image directory as <stem>-<n>.png, the
// first n not taken, and returns the path a comment links it by.
std::string writePastedImage(const eacp::FilePath& home,
                             const std::string& stem,
                             std::string_view png);

std::string makeImageLink(const std::string& path);

// eacp's Files only reads whole files.
std::string readFileFromByteOffset(const eacp::FilePath& path, std::uint64_t offset);

// The line `dv monitor --as <agent>` prints for a round, naming its worktree
// when it isn't `fromWorktree`.
std::string describeRoundForAgent(const StoreLayout& store,
                                  const std::string& fromWorktree,
                                  const std::string& id,
                                  const ReviewFile& round,
                                  const RoundRoute& counts,
                                  const std::string& agent);

std::vector<eacp::FilePath> listDraftFiles(const StoreLayout& store);

int findHighestIdInOtherDrafts(const StoreLayout& store);

class StoreLock
{
public:
    explicit StoreLock(const eacp::FilePath& directory);
    ~StoreLock();

    StoreLock(const StoreLock&) = delete;
    StoreLock& operator=(const StoreLock&) = delete;

private:
    int descriptor = -1;
};

// Linked worktrees' own .diffview dirs from before the shared store, merged
// in and removed; taken ids are renumbered and the rounds say so.
std::vector<std::string> mergeWorktreeStores(const StoreLayout& store);
} // namespace diffview

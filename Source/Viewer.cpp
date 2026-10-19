#include "Agents.h"
#include "Viewer.h"

#include <Text.h>

#include <eacp/Core/App/App.h>
#include <eacp/Core/Process/Process.h>
#include <eacp/Core/Utils/Files.h>

#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace diffview
{
namespace
{
constexpr auto maxUntrackedBytes = std::uintmax_t {1024 * 1024};

std::string readFile(const std::filesystem::path& path)
{
    auto in = std::ifstream {path, std::ios::binary};
    auto buffer = std::stringstream {};
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> splitWords(const std::string& text)
{
    auto words = std::vector<std::string> {};
    auto word = std::string {};

    for (auto c: text + " ")
    {
        if (c != ' ')
            word += c;
        else if (!word.empty())
        {
            words.push_back(word);
            word.clear();
        }
    }

    return words;
}

bool isOurs(const std::string& path)
{
    return path == storeDirectoryName
           || path.starts_with(std::string {storeDirectoryName} + "/");
}

} // namespace

std::int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
        .count();
}

Viewer::Viewer(const Options& optionsToUse)
    : options(optionsToUse)
    , repo(optionsToUse.repo)
    , store(optionsToUse.store)
    , base(optionsToUse.base)
    , pump([this] { tick(); }, eacp::Time::MS {16})
{
    root = ftxui::Renderer([this] { return render(); })
           | ftxui::CatchEvent([this](const ftxui::Event& event)
                               { return onEvent(event); });

    screen.TrackMouse(false);

    // Ctrl-C is ours: quitting must ask about an unsubmitted draft.
    screen.ForceHandleCtrlC(false);
    loop = std::make_unique<ftxui::Loop>(&screen, root);

    loadStore();
    watcher = std::make_unique<RepoWatcher>(
        repo,
        [this](const std::vector<std::string>& changed) { onRepoChanged(changed); });

    if (!isSamePath(store.home.str(), store.worktree.str()))
    {
        createStoreDirectory(store.directory);
        storeWatcher = std::make_unique<RepoWatcher>(
            store.directory.str(),
            [this](const std::vector<std::string>&) { scheduleRefresh(120); });
    }

    for (const auto& note: options.mergeNotes)
        message += (message.empty() ? "" : "; ") + note;

    messageAt = nowMs();

    refresh();
}

Viewer::~Viewer() = default;

void Viewer::tick()
{
    if (loop->HasQuitted())
    {
        eacp::Apps::quit();
        return;
    }

    loop->RunOnce();

    // Highlights fade on a clock, not on an event.
    auto now = nowMs();
    auto fading = tracker.anyRecent(now, recentWindowMs + 1000)
                  || (!message.empty() && now - messageAt < 6000);

    if (fading && now - lastRecentRedraw >= 1000)
    {
        lastRecentRedraw = now;
        redraw();
    }
}

void Viewer::onRepoChanged(const std::vector<std::string>& changed)
{
    auto relevant = changed.empty();

    for (const auto& path: changed)
        if (git::affectsDiff(path))
            relevant = true;

    if (relevant)
        scheduleRefresh(120);
}

void Viewer::scheduleRefresh(int delayMs)
{
    if (refreshScheduled)
        return;

    refreshScheduled = true;
    auto alive = std::weak_ptr<int> {lifetime};

    eacp::Threads::callAfter(eacp::Time::MS {delayMs},
                             [this, alive]
                             {
                                 if (alive.expired())
                                     return;

                                 refreshScheduled = false;
                                 refresh();
                             });
}

eacp::Threads::Async<void> Viewer::refresh()
{
    if (refreshing)
    {
        refreshAgain = true;
        co_return;
    }

    refreshing = true;

    do
    {
        refreshAgain = false;

        auto resolved =
            co_await eacp::Processes::runAsync(git::resolveBase(repo, base));
        auto sha = trim(resolved.output);

        if (resolved.exitCode != 0 || sha.empty())
        {
            if (base == git::BaseSpec {})
            {
                sha = git::emptyTree; // no commits yet
            }
            else
            {
                say("Can't resolve base " + base.label() + "; back to HEAD");
                base = {};
                refreshAgain = true;
                continue;
            }
        }

        auto diffResult = co_await eacp::Processes::runAsync(git::diff(repo, sha));
        auto untrackedResult =
            co_await eacp::Processes::runAsync(git::untracked(repo));

        if (diffResult.exitCode != 0)
        {
            say("git diff failed: " + trim(diffResult.errorOutput));
            continue;
        }

        auto diffs = parseDiff(diffResult.output);

        if (allFiles)
        {
            auto trackedResult =
                co_await eacp::Processes::runAsync(git::tracked(repo));
            allPaths.clear();

            for (const auto* output:
                 {&trackedResult.output, &untrackedResult.output})
                for (auto& path: splitNul(*output))
                    if (!isOurs(path))
                        allPaths.push_back(std::move(path));
        }

        for (const auto& path: splitNul(untrackedResult.output))
        {
            if (isOurs(path))
                continue;

            auto full = std::filesystem::path {repo} / path;
            auto error = std::error_code {};
            auto size = std::filesystem::file_size(full, error);

            if (error)
                continue;

            if (size > maxUntrackedBytes)
            {
                auto file = FileDiff {};
                file.path = path;
                file.status = FileStatus::untracked;
                file.note = "New file, " + std::to_string(size / 1024)
                            + " KB: too big to show";
                finalize(file);
                diffs.push_back(std::move(file));
                continue;
            }

            diffs.push_back(diffForNewFile(path, readFile(full)));
        }

        std::erase_if(diffs, [](const FileDiff& file) { return isOurs(file.path); });
        apply(std::move(diffs), sha);
    } while (refreshAgain);

    refreshing = false;
}

void Viewer::apply(std::vector<FileDiff> diffs, const std::string& sha)
{
    auto list = std::vector<std::string> {};

    for (const auto& file: diffs)
        list.push_back(file.path);

    auto rank = std::map<std::string, std::size_t> {};
    auto order = treeOrder(list);

    for (std::size_t i = 0; i < order.size(); ++i)
        rank[order[i]] = i;

    std::sort(diffs.begin(),
              diffs.end(),
              [&](const FileDiff& a, const FileDiff& b)
              { return rank[a.path] < rank[b.path]; });

    auto baseMoved = sha != baseSha;
    baseSha = sha;
    files = std::move(diffs);
    tracker.update(files, nowMs());

    reloadStoreIfChanged();
    listening = findListeningAgents(store.directory.str());
    unchangedCache.clear();
    auto moved =
        reanchorAll(annotations,
                    files,
                    [this](const std::string& path) { return fileAt(path); });

    if (moved || baseMoved)
        saveStore();

    rebuildTree();

    if (!loaded || fileAt(selectedPath) == nullptr)
    {
        auto previous = std::min(treeCursor, static_cast<int>(treeRows.size()) - 1);
        auto path = std::string {};

        for (auto i = std::max(previous, 0); i < static_cast<int>(treeRows.size());
             ++i)
            if (!treeRows[static_cast<std::size_t>(i)].isDirectory)
            {
                path = treeRows[static_cast<std::size_t>(i)].path;
                break;
            }

        if (path.empty() && !files.empty())
            path = files.front().path;

        loaded = true;
        select(path);
    }
    else
    {
        rebuildRows();
        tracker.markSeen(selectedPath);
    }

    redraw();
}

void Viewer::redraw()
{
    screen.PostEvent(ftxui::Event::Custom);
}

void Viewer::say(std::string text)
{
    message = std::move(text);
    messageAt = nowMs();
    redraw();
}

const FileDiff* Viewer::fileAt(const std::string& path) const
{
    // "" is the General notes.
    if (path.empty())
        return &generalFile;

    if (const auto* changed = changedFile(path))
        return changed;

    // Not in the diff: the file whole, read the first time it is wanted.
    if (auto cached = unchangedCache.find(path); cached != unchangedCache.end())
        return &cached->second;

    auto full = std::filesystem::path {repo} / path;
    auto error = std::error_code {};

    if (!std::filesystem::is_regular_file(full, error))
        return nullptr;

    auto size = std::filesystem::file_size(full, error);
    auto file = FileDiff {};

    if (!error && size > maxUntrackedBytes)
    {
        file.path = path;
        file.status = FileStatus::unchanged;
        file.note =
            "Unchanged, " + std::to_string(size / 1024) + " KB: too big to show";
        finalize(file);
    }
    else
    {
        file = unchangedFile(path, readFile(full));
    }

    return &unchangedCache.emplace(path, std::move(file)).first->second;
}

const FileDiff* Viewer::selectedFile() const
{
    return fileAt(selectedPath);
}

std::vector<std::string> Viewer::paths() const
{
    auto list = std::vector<std::string> {};

    for (const auto& file: files)
        list.push_back(file.path);

    if (allFiles)
    {
        auto changed = std::set<std::string> {list.begin(), list.end()};

        for (const auto& path: allPaths)
            if (!changed.contains(path))
                list.push_back(path);
    }

    return list;
}

const FileDiff* Viewer::changedFile(const std::string& path) const
{
    for (const auto& file: files)
        if (file.path == path)
            return &file;

    return nullptr;
}

std::vector<std::string> Viewer::fileOrder() const
{
    if (!flatBySize)
        return treeOrder(paths());

    // Biggest change first; unchanged files (all-files mode) last.
    auto list = paths();
    auto size = [&](const std::string& path)
    {
        const auto* file = changedFile(path);
        return file == nullptr ? -1 : file->added + file->removed;
    };

    std::stable_sort(list.begin(),
                     list.end(),
                     [&](const std::string& a, const std::string& b)
                     {
                         auto sa = size(a);
                         auto sb = size(b);
                         return sa != sb ? sa > sb : a < b;
                     });
    return list;
}

// The cursor keeps its row by path: a refresh never moves it, so it can't
// undo a keypress. Only select() puts it on the selected file.
void Viewer::rebuildTree()
{
    auto before = std::move(treeRows);
    treeRows.clear();

    if (flatBySize)
    {
        for (const auto& path: fileOrder())
            treeRows.push_back({path, path, 0, false, true});
    }
    else
    {
        treeRows = buildTree(paths(), collapsed);
    }

    auto hasGeneral = std::any_of(annotations.begin(),
                                  annotations.end(),
                                  [](const Annotation& a)
                                  { return a.path.empty() && !a.withdrawn; });

    if (hasGeneral || (loaded && selectedPath.empty()))
        treeRows.insert(treeRows.begin(), TreeRow {"General", "", 0, false, true});

    treeCursor = cursorAfterRebuild(before, treeCursor, treeRows, selectedPath);
}

void Viewer::rebuildRows()
{
    auto key = cursorKey();
    auto offset = diffCursor - diffTop;
    auto* file = selectedFile();

    rows = file != nullptr ? rowsFor(*file) : std::vector<DiffRow> {};
    restoreCursor(key);
    diffTop = std::max(0, diffCursor - offset);
}

void Viewer::select(const std::string& path)
{
    if (path != selectedPath)
    {
        selectedPath = path;
        diffCursor = 0;
        diffTop = 0;
        visualAnchor.reset();

        auto* file = selectedFile();
        rows = file != nullptr ? rowsFor(*file) : std::vector<DiffRow> {};

        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].kind == RowKind::line)
            {
                diffCursor = static_cast<int>(i);
                break;
            }
    }

    for (const auto& parent: parentDirectories(path))
        collapsed.erase(parent);

    tracker.markSeen(path);
    rebuildTree();

    if (auto row = rowOf(treeRows, path); row >= 0)
        treeCursor = row;
}

Viewer::CursorKey Viewer::cursorKey() const
{
    auto key = CursorKey {};

    if (diffCursor < 0 || diffCursor >= static_cast<int>(rows.size()))
        return key;

    const auto& row = rows[static_cast<std::size_t>(diffCursor)];
    key.kind = row.kind;
    key.annotation = row.annotation;
    key.hunk = row.hunk;

    if (row.annotation != 0)
        key.threadOffset =
            findOffsetInThread(rows, static_cast<std::size_t>(diffCursor));

    if (row.kind == RowKind::line)
    {
        if (const auto* file = selectedFile();
            file != nullptr && row.hunk < static_cast<int>(file->hunks.size()))
        {
            const auto& line = file->hunks[static_cast<std::size_t>(row.hunk)]
                                   .lines[static_cast<std::size_t>(row.line)];
            key.newLine = line.newLine;
            key.oldLine = line.oldLine;
            key.text = line.text;
        }
    }

    return key;
}

void Viewer::restoreCursor(const CursorKey& key)
{
    auto count = static_cast<int>(rows.size());

    if (count == 0)
    {
        diffCursor = 0;
        return;
    }

    if (key.annotation != 0)
    {
        if (auto row = findRowInThread(rows, key.annotation, key.threadOffset))
        {
            diffCursor = *row;
            return;
        }
    }

    const auto* file = selectedFile();
    auto best = -1;
    auto bestScore = -1;

    for (auto i = 0; i < count; ++i)
    {
        const auto& row = rows[static_cast<std::size_t>(i)];
        auto score = -1;

        if (key.kind == RowKind::line && row.kind == RowKind::line
            && file != nullptr)
        {
            const auto& line = file->hunks[static_cast<std::size_t>(row.hunk)]
                                   .lines[static_cast<std::size_t>(row.line)];
            auto sameLine = key.newLine > 0 ? line.newLine == key.newLine
                                            : line.oldLine == key.oldLine;

            if (sameLine)
                score = line.text == key.text ? 900 : 500;
            else if (line.text == key.text)
                score = 400;
        }
        else if (key.kind == RowKind::hunkHeader && row.kind == RowKind::hunkHeader
                 && row.hunk == key.hunk)
            score = 300;

        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }

    diffCursor = bestScore >= 0 ? best : std::clamp(diffCursor, 0, count - 1);
}

int Viewer::annotationAtCursor() const
{
    if (diffCursor < 0 || diffCursor >= static_cast<int>(rows.size()))
        return 0;

    const auto& row = rows[static_cast<std::size_t>(diffCursor)];

    if (row.annotation != 0)
        return row.annotation;

    if (row.kind == RowKind::line && diffCursor + 1 < static_cast<int>(rows.size()))
        return rows[static_cast<std::size_t>(diffCursor + 1)].annotation;

    return 0;
}

int Viewer::findNextAnnotationId() const
{
    return std::max({nextAnnotationId(annotations),
                     nextAnnotationId(otherWorktreeAnnotations),
                     highestIdInOtherDrafts + 1});
}

std::optional<Annotation> Viewer::findAnnotationInAnyWorktree(int id) const
{
    for (const auto* group: {&annotations, &otherWorktreeAnnotations})
    {
        for (const auto& note: *group)
        {
            if (note.id == id)
                return note;
        }
    }

    return std::nullopt;
}

std::string Viewer::describeCommentPlace(const Annotation& note) const
{
    auto place = [&]
    {
        if (note.path.empty())
            return std::string {"General"};

        if (note.fileLevel)
            return note.path + " (file)";

        return note.path + ":" + lineLabel(note);
    }();

    if (isSamePath(note.worktree, store.worktree.str()))
        return place;

    return "[" + getWorktreeShortName(note.worktree) + "] " + place;
}

Annotation* Viewer::annotation(int id)
{
    for (auto& item: annotations)
        if (item.id == id)
            return &item;

    return nullptr;
}

std::optional<LineRange> Viewer::hunkRange(int rowIndex) const
{
    auto hunk = -1;

    for (auto i = rowIndex; i >= 0 && hunk < 0; --i)
        hunk = rows[static_cast<std::size_t>(i)].hunk;

    if (hunk < 0)
        return std::nullopt;

    auto range = std::optional<LineRange> {};

    for (const auto& row: rows)
        if (row.kind == RowKind::line && row.hunk == hunk)
        {
            if (!range)
                range = LineRange {row.flat, row.flat};

            range->last = row.flat;
        }

    return range;
}

std::vector<DiffRow> Viewer::rowsFor(const FileDiff& file) const
{
    auto built = buildRows(file, shownAnnotations(), wrapWidth);

    auto hidden = std::count_if(annotations.begin(),
                                annotations.end(),
                                [&](const Annotation& a)
                                {
                                    return a.path == file.path && a.resolved
                                           && !a.withdrawn && !showResolved
                                           && !pinnedThreads.contains(a.id);
                                });

    if (hidden > 0)
        built.insert(
            built.begin() + 1,
            {.kind = RowKind::message,
             .text = "✓ " + std::to_string(hidden) + " resolved · X shows them"});

    return built;
}

std::vector<Annotation> Viewer::shownAnnotations() const
{
    auto shown = std::vector<Annotation> {};

    for (const auto& note: annotations)
        if (!note.withdrawn
            && (showResolved || !note.resolved || pinnedThreads.contains(note.id)))
            shown.push_back(note);

    return shown;
}

int Viewer::openCount() const
{
    return static_cast<int>(std::count_if(annotations.begin(),
                                          annotations.end(),
                                          [](const Annotation& a)
                                          { return !a.resolved; }));
}

void Viewer::moveCursor(int delta)
{
    focus = Pane::diff;
    jumpToRow(diffCursor + delta);
}

void Viewer::jumpToRow(int row)
{
    if (rows.empty())
        return;

    diffCursor = std::clamp(row, 0, static_cast<int>(rows.size()) - 1);
}

void Viewer::moveTree(int delta)
{
    if (treeRows.empty())
        return;

    treeCursor =
        std::clamp(treeCursor + delta, 0, static_cast<int>(treeRows.size()) - 1);
    const auto& row = treeRows[static_cast<std::size_t>(treeCursor)];

    if (!row.isDirectory)
        select(row.path);
}

void Viewer::stepHunk(int direction, int count)
{
    focus = Pane::diff;

    for (auto step = 0; step < count; ++step)
    {
        auto i = diffCursor + direction;

        if (direction < 0)
        {
            // From inside a hunk, [c first goes to its own start, as in vim.
            auto ownHeader = diffCursor;

            while (ownHeader >= 0
                   && rows[static_cast<std::size_t>(ownHeader)].kind
                          != RowKind::hunkHeader)
                --ownHeader;

            i = ownHeader >= 0 && ownHeader + 1 < diffCursor ? ownHeader
                                                             : ownHeader - 1;

            while (i >= 0
                   && rows[static_cast<std::size_t>(i)].kind != RowKind::hunkHeader)
                --i;
        }
        else
        {
            while (i < static_cast<int>(rows.size())
                   && rows[static_cast<std::size_t>(i)].kind != RowKind::hunkHeader)
                ++i;
        }

        if (i < 0 || i >= static_cast<int>(rows.size()))
            break;

        diffCursor = std::min(i + 1, static_cast<int>(rows.size()) - 1);
        diffTop = std::max(0, i - 2);
    }
}

void Viewer::stepFile(int direction, int count)
{
    auto order = fileOrder();

    if (order.empty())
        return;

    auto at = std::find(order.begin(), order.end(), selectedPath);
    auto index = at == order.end() ? 0 : static_cast<int>(at - order.begin());
    auto size = static_cast<int>(order.size());
    index = ((index + direction * count) % size + size) % size;
    select(order[static_cast<std::size_t>(index)]);
}

void Viewer::stepUnseen(int direction)
{
    auto order = fileOrder();
    auto size = static_cast<int>(order.size());
    auto at = std::find(order.begin(), order.end(), selectedPath);
    auto start = at == order.end() ? 0 : static_cast<int>(at - order.begin());

    for (auto step = 1; step <= size; ++step)
    {
        auto index = ((start + direction * step) % size + size) % size;
        const auto& path = order[static_cast<std::size_t>(index)];

        if (path != selectedPath && tracker.isUnseen(path))
        {
            select(path);
            return;
        }
    }

    say("No unseen files");
}

void Viewer::stepAnnotation(int direction, int count)
{
    for (auto step = 0; step < count; ++step)
    {
        auto found = false;

        for (auto i = diffCursor + direction;
             i >= 0 && i < static_cast<int>(rows.size());
             i += direction)
        {
            const auto& row = rows[static_cast<std::size_t>(i)];

            if (row.kind == RowKind::annotationTop
                && (direction > 0
                    || rows[static_cast<std::size_t>(diffCursor)].annotation
                           != row.annotation))
            {
                diffCursor = i;
                focus = Pane::diff;
                found = true;
                break;
            }
        }

        if (found)
            continue;

        auto order = fileOrder();
        auto at = std::find(order.begin(), order.end(), selectedPath);
        auto index = at == order.end() ? 0 : static_cast<int>(at - order.begin());
        auto size = static_cast<int>(order.size());

        for (auto hop = 1; hop <= size; ++hop)
        {
            const auto& path = order[static_cast<std::size_t>(
                ((index + direction * hop) % size + size) % size)];
            auto has =
                std::any_of(annotations.begin(),
                            annotations.end(),
                            [&](const Annotation& a) { return a.path == path; });

            if (!has)
                continue;

            select(path);
            focus = Pane::diff;

            auto target = -1;

            for (auto i = 0; i < static_cast<int>(rows.size()); ++i)
                if (rows[static_cast<std::size_t>(i)].kind == RowKind::annotationTop)
                {
                    target = i;

                    if (direction > 0)
                        break;
                }

            if (target >= 0)
                diffCursor = target;

            found = true;
            break;
        }

        if (!found)
        {
            say("No annotations");
            return;
        }
    }
}

void Viewer::centerCursor()
{
    diffTop = std::max(0, diffCursor - diffHeight / 2);
}

void Viewer::jumpToFlat(const std::string& path, int flat)
{
    select(path);
    focus = Pane::diff;

    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].kind == RowKind::line && rows[i].flat == flat)
        {
            diffCursor = static_cast<int>(i);
            break;
        }

    centerCursor();
}

void Viewer::jumpToAnnotation(int id)
{
    const auto* note = annotation(id);

    if (note == nullptr)
        return;

    select(note->path);
    focus = Pane::diff;

    for (std::size_t i = 0; i < rows.size(); ++i)
        if (rows[i].kind == RowKind::annotationTop && rows[i].annotation == id)
        {
            diffCursor = static_cast<int>(i);
            break;
        }

    centerCursor();
}

void Viewer::requestQuit()
{
    if (draftCount(annotations) > 0)
        modal = Modal::confirmQuit;
    else
        screen.Exit();
}

void Viewer::openInEditor()
{
    const auto* file = selectedFile();

    if (file == nullptr || file->path.empty())
    {
        say("No file open");
        return;
    }

    if (file->status == FileStatus::deleted)
    {
        say("Deleted in the working tree: nothing to edit");
        return;
    }

    auto lineAt = [&](int index)
    {
        const auto& row = rows[static_cast<std::size_t>(index)];

        if (row.kind != RowKind::line)
            return 0;

        return file->hunks[static_cast<std::size_t>(row.hunk)]
            .lines[static_cast<std::size_t>(row.line)]
            .newLine;
    };

    auto line = 0;
    auto count = static_cast<int>(rows.size());

    for (auto i = diffCursor; i < count && line == 0; ++i)
        line = lineAt(i);

    for (auto i = diffCursor; i >= 0 && line == 0; --i)
        line = lineAt(i);

    auto command = std::string {};

    if (const auto* fromEnv = std::getenv("VISUAL"); fromEnv != nullptr && *fromEnv)
        command = fromEnv;
    else if (const auto* editorEnv = std::getenv("EDITOR");
             editorEnv != nullptr && *editorEnv)
        command = editorEnv;
    else
        command = "nvim";

    // $EDITOR may carry its own arguments ("code -w").
    auto words = splitWords(command);
    auto processOptions = eacp::Processes::ProcessOptions {};
    processOptions.executable = words.front();
    processOptions.arguments.assign(words.begin() + 1, words.end());
    processOptions.arguments.push_back("+" + std::to_string(std::max(1, line)));
    processOptions.arguments.push_back(
        (std::filesystem::path {repo} / file->path).string());
    processOptions.workingDirectory = repo;
    processOptions.captureOutput = false;

    auto launched = true;

    // Blocking the main thread is the point: eacp's timer, and with it
    // FTXUI, stays off the terminal until the editor exits.
    screen.WithRestoredIO(
        [&]
        {
            auto child = eacp::Processes::Process {processOptions};
            launched = child.launched();

            if (launched)
                child.wait();
        })();

    if (!launched)
        say("Couldn't start " + words.front());

    refresh();
}

void Viewer::openWithSystem()
{
    auto path = std::string {};

    if (focus == Pane::tree && treeCursor < static_cast<int>(treeRows.size()))
        path = treeRows[static_cast<std::size_t>(treeCursor)].path;
    else if (const auto* file = selectedFile())
        path = file->path;

    if (path.empty())
    {
        say("Not a file");
        return;
    }

    if (!eacp::Files::openWithSystem((std::filesystem::path {repo} / path).string()))
        say("Couldn't open " + path);
}
} // namespace diffview

#pragma once

#include "RepoWatcher.h"

#include <ChangeTracker.h>
#include <DiffRows.h>
#include <FileTree.h>
#include <Git.h>
#include <Keymap.h>
#include <Review.h>
#include <Store.h>
#include <Wake.h>
#include <Telescope.h>
#include <VimEditor.h>

#include <eacp/Core/Threads/Async.h>
#include <eacp/Core/Threads/Timer.h>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace diffview
{
constexpr auto recentWindowMs = std::int64_t {10000};

std::int64_t nowMs();

struct Options
{
    std::string repo;
    StoreLayout store;
    std::vector<std::string> mergeNotes;
    std::function<WakeConfig()> readWakeConfig = [] { return WakeConfig {}; };
    std::function<void(const WakeLaunch&)> launchWake = [](const WakeLaunch&) {};
    git::BaseSpec base;
};

// eacp owns the main thread and a timer pumps FTXUI's loop; git results, the
// watcher and FTXUI all land on that thread, so nothing here is locked.
class Viewer
{
public:
    explicit Viewer(const Options& options);
    ~Viewer();

private:
    enum class Pane
    {
        tree,
        diff
    };

    // The comment editor isn't one: it is drawn inline (`editing`).
    enum class Modal
    {
        none,
        help,
        picker,
        confirmSubmit,
        confirmQuit
    };

    enum class Prompt
    {
        none,
        search,
        command
    };

    enum class PickerKind
    {
        files,
        grep,
        comments,
        base
    };

    struct CursorKey
    {
        RowKind kind = RowKind::line;
        int newLine = 0;
        int oldLine = 0;
        std::string text;
        int annotation = 0;
        int threadOffset = 0;
        int hunk = -1;
    };

    struct SearchHit
    {
        int row = 0;
        std::size_t start = 0;
        std::size_t length = 0;
    };

    // Viewer.cpp
    void tick();
    void onRepoChanged(const std::vector<std::string>& paths);
    void scheduleRefresh(int delayMs);
    eacp::Threads::Async<void> refresh();
    void apply(std::vector<FileDiff> diffs, const std::string& sha);
    void redraw();
    void say(std::string message);
    void requestQuit();

    const FileDiff* selectedFile() const;
    const FileDiff* fileAt(const std::string& path) const;
    std::vector<std::string> paths() const;
    const FileDiff* changedFile(const std::string& path) const;
    std::vector<std::string> fileOrder() const;
    void rebuildTree();
    void rebuildRows();
    void select(const std::string& path);
    CursorKey cursorKey() const;
    void restoreCursor(const CursorKey& key);
    int annotationAtCursor() const;
    Annotation* annotation(int id);
    int findNextAnnotationId() const;
    std::string wakeAgentsFor(const ReviewFile& round, const std::string& path);
    std::optional<Annotation> findAnnotationInAnyWorktree(int id) const;
    std::string describeCommentPlace(const Annotation& note) const;
    std::optional<LineRange> hunkRange(int row) const;
    int openCount() const;
    std::vector<Annotation> shownAnnotations() const;
    std::vector<DiffRow> rowsFor(const FileDiff& file) const;
    void jumpToFlat(const std::string& path, int flat);
    void jumpToAnnotation(int id);

    void moveCursor(int delta);
    void moveTree(int delta);
    void jumpToRow(int row);
    void centerCursor();
    void stepHunk(int direction, int count);
    void stepFile(int direction, int count);
    void stepAnnotation(int direction, int count);
    void stepUnseen(int direction);
    void openInEditor();
    void openWithSystem();

    // Comments.cpp
    void loadStore();
    bool reloadStoreIfChanged();
    std::string roundsSignature() const;
    void saveStore();
    void submitReview();
    void sendReply(Annotation& note, const std::string& text);
    static std::string threadKey(const Annotation& note);
    bool hasUnseenAgentReply(const Annotation& note) const;
    void pinUnseenThreads();
    void markSeen(const Annotation& note);
    void annotateAtCursor(bool wholeHunk);
    void commentOnFile();
    void commentGeneral();
    void openEditor(std::string title, const std::string& text);
    bool onEditorEvent(const ftxui::Event& event);
    bool commitEditor();
    void closeEditor(bool keepPastedImages);
    bool isCursorInThread(int id) const;
    void moveCursorToEndOfThread(int id);
    void pasteIntoEditor();
    void editAtCursor();
    void deleteAtCursor();
    void toggleResolvedAtCursor();
    void replyAtCursor();
    bool completeMention();
    void clearResolved();
    void removeImages(const Annotation& annotation);

    // Input.cpp
    bool onEvent(const ftxui::Event& event);
    void run(const Command& command);
    void openPrompt(Prompt kind, bool forward);
    bool onPromptEvent(const std::string& key);
    void updateIncrementalSearch();
    std::optional<SearchHit> findFrom(int row,
                                      bool forward,
                                      const std::string& needle,
                                      bool& wrapped) const;
    void searchAgain(bool sameDirection);
    void runCommandLine(const std::string& line);
    void completeCommandLine();
    void openPicker(PickerKind kind);
    eacp::Threads::Async<void> loadBaseChoices();
    bool onPickerEvent(const std::string& key);
    void refreshPickerItems();
    void acceptPicker();
    bool onConfirmEvent(const std::string& key);

    // Render.cpp
    ftxui::Element render();
    ftxui::Element renderTree(int width, int height);
    ftxui::Element renderDiff(int width, int height);
    ftxui::Element renderRow(const DiffRow& row, int index, int width);
    ftxui::Element renderThreadRow(const DiffRow& row,
                                   const Annotation& note,
                                   int index,
                                   ftxui::Element shown);
    ftxui::Element renderStatus();
    ftxui::Element renderPrompt();
    ftxui::Element renderModal();
    ftxui::Element renderPicker();
    ftxui::Element renderConfirm();
    ftxui::Element renderHelp();
    std::optional<LineRange> editorTarget() const;
    ftxui::Element renderEditor(int width, int boxHeight);
    int editorHeight(int width);

    Options options;
    std::string repo;
    StoreLayout store;
    git::BaseSpec base;
    std::string baseSha;

    std::vector<FileDiff> files;

    // The tree's modes: every file in the repo, not just the changed ones
    // (unchanged ones read lazily into the cache); a flat list by size.
    bool allFiles = false;
    bool flatBySize = false;
    std::vector<std::string> allPaths;
    mutable std::map<std::string, FileDiff> unchangedCache;
    FileDiff generalFile = []
    {
        auto general = FileDiff {};
        general.note = "Notes about the work as a whole. gc or :note adds one.";
        return general;
    }();
    ChangeTracker tracker;
    bool loaded = false;

    // This worktree's; other worktrees' anchor in their own checkouts, so
    // they are only listed.
    std::vector<Annotation> annotations;
    std::vector<Annotation> otherWorktreeAnnotations;
    int highestIdInOtherDrafts = 0;
    std::vector<std::string> rounds;
    std::vector<std::string> listening;

    // What the files said when last read or written, to tell our own writes
    // from a hand edit.
    std::string lastDraftText;
    std::string lastRoundsSignature;
    bool showResolved = false;

    std::set<std::string> seenThreads;
    std::set<int> pinnedThreads;

    std::set<std::string> collapsed;
    std::vector<TreeRow> treeRows;
    int treeCursor = 0;
    int treeTop = 0;

    std::string selectedPath;
    std::vector<DiffRow> rows;
    int diffCursor = 0;
    int diffTop = 0;
    int diffHeight = 20;
    int treeHeight = 20;
    int wrapWidth = 60;

    Pane focus = Pane::tree;
    KeyParser keys;
    std::optional<int> visualAnchor;
    Modal modal = Modal::none;

    bool editing = false;
    vim::Editor editor;
    std::string editorTitle;
    int editingId = 0;
    std::optional<int> replyCommentId;
    std::optional<Annotation> pendingAnnotation;
    std::vector<std::string> pastedImages;
    std::optional<LineRange> drawnTarget;
    bool editorJustOpened = false;

    Prompt prompt = Prompt::none;
    telescope::LineEdit promptLine;
    bool searchForward = true;
    Pane searchPane = Pane::diff;
    int searchOriginCursor = 0;
    std::string searchOriginPath; // the tree's: its rows move under a refresh
    int searchOriginTop = 0;
    std::optional<SearchHit> liveHit;
    std::string lastSearch;
    bool lastSearchForward = true;

    telescope::Picker picker;
    std::vector<git::BaseChoice> baseChoices;
    telescope::LineEdit pickerLine;
    PickerKind pickerKind = PickerKind::files;

    std::string message;
    std::int64_t messageAt = 0;

    bool refreshing = false;
    bool refreshAgain = false;
    bool refreshScheduled = false;
    std::int64_t lastRecentRedraw = 0;
    std::shared_ptr<int> lifetime = std::make_shared<int>(0);

    ftxui::App screen = ftxui::App::Fullscreen();
    ftxui::Component root;
    std::unique_ptr<ftxui::Loop> loop;

    std::unique_ptr<RepoWatcher> watcher;
    std::unique_ptr<RepoWatcher> storeWatcher;
    eacp::Threads::Timer pump;
};
} // namespace diffview

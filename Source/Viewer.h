#pragma once

#include "NarrowParts.h"
#include "RepoWatcher.h"
#include "UsageLog.h"

#include <ChangeTracker.h>
#include <DiffRows.h>
#include <FileTree.h>
#include <Git.h>
#include <Highlight.h>
#include <Keymap.h>
#include <NarrowLayout.h>
#include <PathFilter.h>
#include <Review.h>
#include <Store.h>
#include <Telescope.h>
#include <VimEditor.h>

#include <eacp/Core/Threads/Async.h>
#include <eacp/Core/Threads/Timer.h>

#include <ftxui/component/app.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/loop.hpp>

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
    git::BaseSpec base;
    LayoutRequest layout = LayoutRequest::byWidth;

    // Read from a pipe (`git diff main | dv`) and shown as is: git never
    // recomputes it.
    std::optional<std::vector<FileDiff>> pipedFiles;

    // Outside a git work tree there is no review store to keep comments in.
    bool isReadOnly = false;
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
        filter,
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
    void onRepoChanged(const std::vector<std::string>& changed);
    void scheduleRefresh(int delayMs);
    void refresh();
    eacp::Threads::Async<void> refreshFromGit();
    void apply(std::vector<FileDiff> diffs, const std::string& sha);
    static void logSlowFileRead(std::int64_t bytes, std::int64_t readMs);
    void logSlowRender(std::int64_t renderMs) const;
    void logRefresh(std::string_view cause,
                    int fileCount,
                    std::int64_t gitMs,
                    std::int64_t totalMs);
    void redraw();
    void say(std::string text);
    void requestQuit();
    std::string_view getPaneName() const;
    void feedKeyAndRunCommand(const std::string& key);
    vim::Outcome feedEditorAndLogModeChange(const std::string& key);
    std::string_view getPromptName() const;
    std::string_view getPickerName() const;

    OptionalFileDiff selectedFile() const;
    OptionalFileDiff findFile(const std::string& path) const;
    OptionalFileDiff findOrReadFile(const std::string& path);
    std::vector<std::string> paths() const;
    std::vector<std::string> visiblePaths() const;
    void setPathFilter(std::optional<PathFilter> filter);
    void clearPathFilter();
    void updateLiveFilter();
    OptionalFileDiff changedFile(const std::string& path) const;
    std::vector<std::string> fileOrder() const;
    void rebuildTree();
    void rebuildRows();
    void toggleFullContext();
    void rebuildRows(const CursorKey& key, int offset);
    void select(std::string path);
    CursorKey cursorKey() const;
    void restoreCursor(const CursorKey& key);
    int annotationAtCursor() const;
    Annotation* annotation(int id);
    int findNextAnnotationId() const;
    std::optional<Annotation> findAnnotationInAnyWorktree(int id) const;
    std::string describeCommentPlace(const Annotation& note) const;
    std::optional<LineRange> hunkRange(int row) const;
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

    // PipedDiff.cpp
    bool isShowingPipedDiff() const;
    bool refuseForPipedDiff(std::string_view feature);
    bool refuseCommentWhenReadOnly();
    std::string describeBase() const;
    void reloadReviewStoreUnlessReadOnly();

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
    void loadThreadFolds();
    void setThreadView(int id, ThreadView view);
    void setEveryThreadView(ThreadView view);
    void toggleFoldAtCursor();
    bool isCursorOn(RowKind kind) const;
    void annotateAtCursor(bool wholeHunk);
    void commentOnFile();
    void commentGeneral();
    void openEditor(std::string title, const std::string& text);
    bool onEditorEvent(const ftxui::Event& event);
    void applyEditorOutcome(vim::Outcome outcome);
    void saveEditor();
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
    void removeImages(const Annotation& note) const;

    // Input.cpp
    bool onEvent(const ftxui::Event& event);
    void run(const Command& command);
    bool hasYankableRows() const;
    void yankSelectionOrLine();
    void openPrompt(Prompt kind, bool forward);
    bool onPromptEvent(const std::string& key);
    void updateIncrementalSearch();
    std::optional<SearchHit> findFrom(int row,
                                      bool forward,
                                      const std::string& needle,
                                      bool& wrapped) const;
    void searchAgain(bool sameDirection);
    void runCommandLine(const std::string& line);
    void toggleAllFiles();
    void refreshOnRequest();
    void changeBase(const std::string& text);
    void completeCommandLine();
    void openPicker(PickerKind kind);
    eacp::Threads::Async<void> loadBaseChoices();
    bool onPickerEvent(const std::string& key);
    void refreshPickerItems();
    void acceptPicker();
    bool onConfirmEvent(const std::string& key);

    // NarrowInput.cpp
    bool onEventInCurrentLayout(const ftxui::Event& event);
    void onNarrowMouseEvent(ftxui::Event event);
    void runTapAction(const TapTarget& target);
    void openTreeEntry(int index);
    void selectDiffRow(int index);
    void goBackOnePage();
    void scrollNarrowPage(int lines);
    void setMouseReporting(bool shouldReport);

    // NarrowRender.cpp
    ftxui::Element renderInCurrentLayout();
    NarrowPage findCurrentNarrowPage() const;
    ftxui::Element renderNarrow(ScreenArea page);
    ftxui::Element renderNarrowPage(ScreenArea page);
    ftxui::Element renderNarrowFooter(const std::string& hint,
                                      const std::vector<NarrowButton>& buttons,
                                      ScreenArea page);
    std::string describeSubmitButton() const;
    ftxui::Element renderNarrowFiles(ScreenArea page);
    ftxui::Element renderNarrowFileList(ScreenArea body);
    std::vector<ftxui::Element>
        renderNarrowFileEntry(const TreeRow& row, int index, int entryHeight);
    std::vector<ftxui::Element>
        renderNarrowDirectoryLines(const TreeRow& row, const NarrowEntryLead& lead);
    std::vector<ftxui::Element>
        renderNarrowGeneralLines(const NarrowEntryLead& lead);
    std::vector<ftxui::Element> renderNarrowFileLines(const TreeRow& row,
                                                      const NarrowEntryLead& lead);
    ftxui::Element renderNarrowDiff(ScreenArea page);
    NarrowHeader describeNarrowDiffHeader() const;
    ftxui::Element renderNarrowDiffBody(ScreenArea body);
    int countNarrowRowHeight(const DiffRow& row, int width) const;
    std::vector<ftxui::Element>
        renderNarrowDiffRow(const DiffRow& row, int index, int width);
    ftxui::Element renderNarrowHunkHeader(const DiffRow& row) const;
    std::vector<ftxui::Element>
        renderNarrowLine(const DiffRow& row, int index, int width);
    ftxui::Element renderNarrowComment(ScreenArea page);
    std::vector<ftxui::Element> renderNarrowQuote(int width);
    bool isQuotedInComment(const DiffRow& row) const;
    ftxui::Element renderNarrowConfirm(ScreenArea page);
    ftxui::Element renderNarrowInfoLine(const std::string& hint);
    std::string describeOpenAtSelection();

    // Render.cpp
    ftxui::Element render();
    ftxui::Element renderTree(int width, int height);
    std::string describeFileListTitle() const;
    int countOpenComments(const std::string& path) const;
    int countOpenGeneralNotes() const;
    ftxui::Element renderDiff(int width, int height);
    enum class RowEmphasis
    {
        none,
        cursor,
        selected
    };

    RowEmphasis findRowEmphasis(const DiffRow& row, int index) const;
    const std::vector<Token>& findLineTokens(const DiffRow& row) const;
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
    std::string describeEditorKeys() const;

    Options options;
    std::string repo;
    StoreLayout store;
    git::BaseSpec base;
    std::string baseSha;

    std::vector<FileDiff> files;

    // The tree's modes: every file in the repo, not just the changed ones
    // (unchanged ones read by findOrReadFile, kept until the next apply); a
    // flat list by size.
    bool allFiles = false;
    bool fullContext = false;
    bool flatBySize = false;
    std::vector<std::string> allPaths;
    std::map<std::string, FileDiff> unchangedFiles;
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
    ThreadFolds threadFolds;

    std::set<std::string> collapsed;
    std::vector<TreeRow> treeRows;
    int treeCursor = 0;
    int treeTop = 0;

    std::string selectedPath;
    std::vector<DiffRow> rows;
    FileTokens lineTokens;
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

    std::optional<PathFilter> pathFilter;
    std::string filterBeforePrompt;
    bool isFilterPromptInvalid = false;
    bool lastSearchForward = true;

    telescope::Picker picker;
    std::vector<git::BaseChoice> baseChoices;
    telescope::LineEdit pickerLine;
    PickerKind pickerKind = PickerKind::files;
    UsageTimer pickerTimer;
    int pickerKeyCount = 0;
    UsageTimer promptTimer;
    UsageTimer editorTimer;

    std::string message;
    std::int64_t messageAt = 0;

    bool refreshing = false;
    bool refreshAgain = false;
    bool refreshScheduled = false;
    std::string_view refreshCause = "launch";
    std::int64_t lastRecentRedraw = 0;
    std::shared_ptr<int> lifetime = std::make_shared<int>(0);

    ftxui::App screen = ftxui::App::Fullscreen();
    ftxui::Component root;
    std::unique_ptr<ftxui::Loop> loop;

    // The narrow layout's: what the last frame drew where, and how far each
    // page is scrolled, in screen lines.
    bool isNarrow = false;
    bool isMouseReporting = false;
    bool shouldRevealCursor = true;
    int narrowFilesScroll = 0;
    int narrowDiffScroll = 0;
    std::vector<TapTarget> tapTargets;
    std::string threadIndent;

    std::unique_ptr<RepoWatcher> watcher;
    std::unique_ptr<RepoWatcher> storeWatcher;
    eacp::Threads::Timer pump;
};
} // namespace diffview

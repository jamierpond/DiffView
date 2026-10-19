#include "Clipboard.h"
#include "Viewer.h"

#include <Text.h>

#include <eacp/Core/App/Clipboard.h>
#include <eacp/Core/Utils/File.h>
#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <cctype>

namespace diffview
{
namespace
{
std::string editorKey(const ftxui::Event& event)
{
    using ftxui::Event;

    if (event == Event::Return)
        return "Enter";
    if (event == Event::Escape)
        return "Esc";
    if (event == Event::Backspace)
        return "Backspace";
    if (event == Event::Delete)
        return "Delete";
    if (event == Event::Tab)
        return "Tab";
    if (event == Event::ArrowLeft)
        return "Left";
    if (event == Event::ArrowRight)
        return "Right";
    if (event == Event::ArrowUp)
        return "Up";
    if (event == Event::ArrowDown)
        return "Down";
    if (event == Event::Home)
        return "Home";
    if (event == Event::End)
        return "End";
    if (event == Event::CtrlS)
        return "C-s";
    if (event == Event::CtrlV)
        return "C-v";
    if (event == Event::CtrlC)
        return "C-c";
    if (event == Event::CtrlR)
        return "C-r";
    if (event == Event::CtrlW)
        return "C-w";
    if (event == Event::CtrlU)
        return "C-u";
    if (event == Event::CtrlH)
        return "C-h";
    if (event.is_character())
        return event.character();

    return {};
}

std::string trimmedEnd(std::string text)
{
    while (!text.empty()
           && (text.back() == '\n' || text.back() == ' ' || text.back() == '\t'))
        text.pop_back();

    return text;
}
} // namespace

std::string Viewer::roundsSignature() const
{
    const auto& storeDirectory = store.directory;
    auto signature = std::string {};
    auto describeFile = [](const eacp::FilePath& path)
    {
        auto file = eacp::File {path};
        return path.str() + ":" + std::to_string(file.size()) + ":"
               + std::to_string(file.modificationTime()) + "\n";
    };

    for (const auto& id: listRoundIdsInOrder(storeDirectory))
        signature += describeFile(storeDirectory / reviewDirectory / (id + ".md"));

    for (const auto& draft: listDraftFiles(store))
        signature += describeFile(draft);

    return signature + eacp::Files::readFile(storeDirectory / latestReviewFileName);
}

void Viewer::loadStore()
{
    const auto& storeDirectory = store.directory;
    auto roundFiles = readRounds(store);
    auto latest = readReviewOrEmpty(storeDirectory / latestReviewFileName);

    for (auto& round: roundFiles)
    {
        if (round.title != latest.title)
            continue;

        if (auto resolved = resolutionsFrom(latest, round))
        {
            writeText(storeDirectory / reviewDirectory / (resolved->title + ".md"),
                      toMarkdown(*resolved));
            round = std::move(*resolved);
        }
    }

    lastDraftText = eacp::Files::readFile(store.worktreeDirectory / draftFileName);
    auto draft = fromMarkdown(lastDraftText);
    draft.worktree = store.worktree.str();
    highestIdInOtherDrafts = findHighestIdInOtherDrafts(store);
    auto merged = mergeReviews(roundFiles, draft, highestIdInOtherDrafts);

    annotations.clear();
    otherWorktreeAnnotations.clear();

    for (auto& note: merged.annotations)
    {
        if (isSamePath(note.worktree, store.worktree.str()))
            annotations.push_back(std::move(note));
        else
            otherWorktreeAnnotations.push_back(std::move(note));
    }

    rounds = std::move(merged.rounds);
    lastRoundsSignature = roundsSignature();
    pinUnseenThreads();

    if (merged.renumbered)
        saveStore();
}

bool Viewer::reloadStoreIfChanged()
{
    auto draftText = eacp::Files::readFile(store.worktreeDirectory / draftFileName);

    if (draftText == lastDraftText && roundsSignature() == lastRoundsSignature)
        return false;

    loadStore();
    say("Reloaded the review files (changed on disk)");
    return true;
}

void Viewer::saveStore()
{
    auto text = toMarkdown(draftFile(annotations, base.label(), baseSha));

    if (text == lastDraftText)
        return;

    if (draftCount(annotations) == 0
        && !eacp::Files::exists(store.worktreeDirectory))
        return;

    createStoreDirectory(store.directory);
    eacp::Files::createDirectories(store.worktreeDirectory);
    writeText(store.worktreeDirectory / draftFileName, text);
    lastDraftText = text;
}

void Viewer::submitReview()
{
    modal = Modal::none;

    if (draftCount(annotations) == 0)
    {
        say("Nothing to submit: no new or changed comments");
        return;
    }

    const auto& storeDirectory = store.directory;
    auto id = nextRoundId(rounds, nowMs());
    auto round = submitDraft(annotations, id, base.label(), baseSha);
    round.worktree = store.worktree.str();
    auto review = toMarkdown(round);

    eacp::Files::createDirectories(storeDirectory / reviewDirectory);
    writeText(storeDirectory / reviewDirectory / (id + ".md"), review);
    writeText(storeDirectory / latestReviewFileName, review);

    rounds.push_back(id);
    lastRoundsSignature = roundsSignature();
    saveStore();
    rebuildRows();
    say("Submitted " + std::to_string(round.comments.size()) + " comments → "
        + (storeDirectory / latestReviewFileName).str());
}

void Viewer::sendReply(Annotation& note, const std::string& text)
{
    const auto& storeDirectory = store.directory;
    auto sent = mergeReviews(readRounds(store), {}).annotations;
    auto thread = std::find_if(sent.begin(),
                               sent.end(),
                               [&](const Annotation& a) { return a.id == note.id; });

    if (thread == sent.end())
        return;

    auto id = nextRoundId(rounds, nowMs());
    auto time = formatReplyTime(nowMs());
    auto round = makeReplyRound(*thread, text, time, id, base.label(), baseSha);
    round.worktree = store.worktree.str();

    eacp::Files::createDirectories(storeDirectory / reviewDirectory);
    writeText(storeDirectory / reviewDirectory / (id + ".md"), toMarkdown(round));

    note.replies.push_back({reviewerName, time, text, false});
    note.round = id;
    note.owner = round.comments.front().owner;

    auto unchanged = note.text == thread->text && note.resolved == thread->resolved
                     && note.withdrawn == thread->withdrawn;

    if (unchanged)
    {
        note.draft = false;
        note.resolved = false;
        note.withdrawn = false;
        note.reopened = round.comments.front().reopened;
    }

    rounds.push_back(id);
    lastRoundsSignature = roundsSignature();

    auto owner = findListeningOwner(note, listening);
    auto to = owner.empty() ? std::string {"every agent"} : "@" + owner;
    say("Sent reply on #" + std::to_string(note.id) + " → " + to);
}

std::string Viewer::threadKey(const Annotation& note)
{
    return std::to_string(note.id) + " " + std::to_string(note.replies.size());
}

bool Viewer::hasUnseenAgentReply(const Annotation& note) const
{
    return note.resolved && !note.withdrawn && !note.replies.empty()
           && note.replies.back().author != reviewerName
           && !seenThreads.contains(threadKey(note));
}

void Viewer::pinUnseenThreads()
{
    auto seen = eacp::Files::readFile(store.worktreeDirectory / seenFileName);

    for (const auto& line: splitLines(seen))
        seenThreads.insert(line);

    loadThreadFolds();

    for (const auto& note: annotations)
    {
        if (hasUnseenAgentReply(note))
            pinnedThreads.insert(note.id);
    }
}

void Viewer::markSeen(const Annotation& note)
{
    if (!hasUnseenAgentReply(note))
        return;

    seenThreads.insert(threadKey(note));
    eacp::Files::appendFile(store.worktreeDirectory / seenFileName,
                            threadKey(note) + "\n");
}

void Viewer::loadThreadFolds()
{
    threadFolds = parseThreadFolds(
        eacp::Files::readFile(store.worktreeDirectory / foldsFileName));
}

void Viewer::setThreadView(int id, ThreadView view)
{
    const auto* note = annotation(id);

    if (note == nullptr)
        return;

    if (view == ThreadView::recent)
        threadFolds.erase(id);
    else
        threadFolds[id] = {view, note->replies.size()};

    eacp::Files::createDirectories(store.worktreeDirectory);
    writeText(store.worktreeDirectory / foldsFileName,
              formatThreadFolds(threadFolds));
    rebuildRows();
}

void Viewer::setEveryThreadView(ThreadView view)
{
    for (const auto& note: annotations)
        setThreadView(note.id, view);
}

bool Viewer::isCursorOn(RowKind kind) const
{
    if (diffCursor < 0 || diffCursor >= static_cast<int>(rows.size()))
        return false;

    return rows[static_cast<std::size_t>(diffCursor)].kind == kind;
}

void Viewer::toggleFoldAtCursor()
{
    auto id = annotationAtCursor();

    if (findFold(threadFolds, id).view == ThreadView::collapsed)
        setThreadView(id, ThreadView::recent);
    else
        setThreadView(id, ThreadView::collapsed);
}

void Viewer::annotateAtCursor(bool wholeHunk)
{
    auto selected = selectedFile();

    if (!selected || rows.empty())
    {
        say("Nothing to annotate");
        return;
    }

    const auto& file = selected->get();

    auto range = std::optional<LineRange> {};
    const auto& row = rows[static_cast<std::size_t>(diffCursor)];

    if (visualAnchor)
    {
        auto from = std::min(*visualAnchor, diffCursor);
        auto to = std::max(*visualAnchor, diffCursor);

        for (auto i = from; i <= to; ++i)
        {
            const auto& r = rows[static_cast<std::size_t>(i)];

            if (r.kind != RowKind::line)
                continue;

            if (!range)
                range = LineRange {r.flat, r.flat};

            range->last = r.flat;
        }

        visualAnchor.reset();
    }
    else if (row.kind == RowKind::fileHeader)
    {
        commentOnFile();
        return;
    }
    else if (wholeHunk || row.kind == RowKind::hunkHeader)
    {
        range = hunkRange(diffCursor);
    }
    else if (row.kind == RowKind::line)
    {
        if (annotation(annotationAtCursor()) != nullptr)
        {
            editAtCursor();
            return;
        }

        range = LineRange {row.flat, row.flat};
    }
    else if (row.annotation != 0)
    {
        editAtCursor();
        return;
    }

    if (!range)
    {
        say("Nothing to annotate here");
        return;
    }

    pendingAnnotation = makeAnnotation(file, *range, findNextAnnotationId(), {});
    editingId = 0;
    openEditor("New comment · " + file.path + ":" + lineLabel(*pendingAnnotation),
               {});
}

void Viewer::commentOnFile()
{
    auto selected = selectedFile();

    if (!selected || selected->get().path.empty())
    {
        commentGeneral();
        return;
    }

    const auto& file = selected->get();

    visualAnchor.reset();
    pendingAnnotation = makeFileAnnotation(file.path, findNextAnnotationId(), {});
    editingId = 0;
    diffCursor = 0;
    openEditor("New comment · " + file.path + " (whole file)", {});
}

void Viewer::commentGeneral()
{
    visualAnchor.reset();
    pendingAnnotation = makeFileAnnotation({}, findNextAnnotationId(), {});
    editingId = 0;
    select({});
    diffCursor = 0;
    openEditor("New general note", {});
}

void Viewer::openEditor(std::string title, const std::string& text)
{
    editorTitle = std::move(title);
    editor.setText(text);
    pastedImages.clear();

    if (text.empty())
        editor.feed("i");
    else
    {
        editor.feed("G");
        editor.feed("$");
    }

    editing = true;
    editorJustOpened = true;
    focus = Pane::diff;
}

bool Viewer::onEditorEvent(const ftxui::Event& event)
{
    if (event == ftxui::Event::Custom)
        return false;

    auto key = editorKey(event);

    if (key.empty())
        return true;

    if (key == "Tab" && editor.mode() == vim::Mode::insert && completeMention())
        return true;

    switch (editor.feed(key))
    {
        case vim::Outcome::none:
            break;
        case vim::Outcome::save:
            if (replyCommentId)
            {
                commitEditor();
                closeEditor(true);
            }
            else if (commitEditor())
                say("Saved");
            break;
        case vim::Outcome::saveAndClose:
            commitEditor();
            closeEditor(true);
            break;
        case vim::Outcome::close:
            closeEditor(true);
            break;
        case vim::Outcome::discard:
            closeEditor(false);
            say("Discarded");
            break;
        case vim::Outcome::paste:
            pasteIntoEditor();
            break;
    }

    return true;
}

bool Viewer::commitEditor()
{
    auto text = trimmedEnd(editor.text());
    auto linked = imagesIn(text);

    auto removeUnlinked = [&](const std::vector<std::string>& candidates)
    {
        for (const auto& path: candidates)
        {
            if (std::find(linked.begin(), linked.end(), path) == linked.end())
                eacp::Files::removeFile(store.home / path);
        }
    };

    if (editingId != 0)
    {
        auto* existing = annotation(editingId);

        if (existing == nullptr)
            return false;

        // A sent round may still link the old images.
        removeUnlinked(pastedImages);

        if (existing->round.empty())
            removeUnlinked(imagesIn(existing->text));

        if (existing->text != text)
        {
            existing->text = text;
            existing->draft = true;
        }
    }
    else if (replyCommentId)
    {
        auto* note = annotation(*replyCommentId);

        if (note == nullptr)
            return false;

        removeUnlinked(pastedImages);
        auto pending = findPendingReply(*note);

        auto dropPending = [&]
        {
            auto at = static_cast<std::ptrdiff_t>(*pending);
            note->replies.erase(note->replies.begin() + at);
        };

        if (!note->round.empty())
        {
            if (pending)
                dropPending();

            if (!text.empty())
                sendReply(*note, text);
            else if (!pending)
                return false;
        }
        else if (pending && text.empty())
        {
            dropPending();
        }
        else if (pending)
        {
            note->replies[*pending].text = text;
        }
        else if (!text.empty())
        {
            note->replies.push_back(
                {reviewerName, formatReplyTime(nowMs()), text, true});
            note->draft = true;
        }
        else
        {
            return false;
        }
    }
    else if (pendingAnnotation)
    {
        if (text.empty())
            return false;

        removeUnlinked(pastedImages);
        pendingAnnotation->text = text;
        annotations.push_back(*pendingAnnotation);
        editingId = pendingAnnotation->id;
        pendingAnnotation.reset();
    }

    pastedImages.clear();
    editor.markSaved();
    saveStore();
    rebuildRows();
    return true;
}

void Viewer::closeEditor(bool)
{
    const auto* saved = editingId != 0 ? annotation(editingId) : nullptr;
    auto linked = std::vector<std::string> {};

    if (saved != nullptr)
        linked = imagesIn(saved->text);

    for (const auto& path: pastedImages)
    {
        if (std::find(linked.begin(), linked.end(), path) == linked.end())
            eacp::Files::removeFile(store.home / path);
    }

    auto id = editingId != 0 ? editingId : replyCommentId.value_or(0);
    auto wasReplying = replyCommentId.has_value();
    editing = false;
    editingId = 0;
    replyCommentId.reset();
    pendingAnnotation.reset();
    pastedImages.clear();
    rebuildRows();

    if (id == 0)
        return;

    if (wasReplying)
    {
        moveCursorToEndOfThread(id);
        return;
    }

    if (isCursorInThread(id))
        return;

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        if (isThreadStart(rows[i]) && rows[i].annotation == id)
        {
            diffCursor = static_cast<int>(i);
            break;
        }
    }
}

bool Viewer::isCursorInThread(int id) const
{
    if (diffCursor < 0 || diffCursor >= static_cast<int>(rows.size()))
        return false;

    return rows[static_cast<std::size_t>(diffCursor)].annotation == id;
}

void Viewer::moveCursorToEndOfThread(int id)
{
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        auto isLastText =
            rows[i].annotation == id && rows[i].kind == RowKind::annotationText
            && i + 1 < rows.size() && rows[i + 1].kind == RowKind::annotationBottom;

        if (isLastText)
        {
            diffCursor = static_cast<int>(i);
            return;
        }
    }
}

void Viewer::pasteIntoEditor()
{
    if (auto png = clipboard::imagePng())
    {
        auto id = [&]
        {
            if (editingId != 0)
                return editingId;
            if (replyCommentId)
                return *replyCommentId;
            if (pendingAnnotation)
                return pendingAnnotation->id;

            return findNextAnnotationId();
        }();
        auto dir = store.home / imageDirectory;
        eacp::Files::createDirectories(dir);

        auto name = std::string {};

        for (auto n = 1;; ++n)
        {
            name = std::to_string(id) + "-" + std::to_string(n) + ".png";

            if (!eacp::Files::exists(dir / name))
                break;
        }

        writeText(dir / name, *png);

        auto path = std::string {imageDirectory} + "/" + name;
        pastedImages.push_back(path);
        editor.insertText("![image](" + path + ")");
        say("Pasted image " + path);
        return;
    }

    auto text = eacp::Clipboard::getText();

    if (text.empty())
    {
        say("Clipboard is empty");
        return;
    }

    editor.insertText(text);
}

void Viewer::editAtCursor()
{
    auto id = annotationAtCursor();
    auto* existing = annotation(id);

    if (existing == nullptr)
    {
        say("No comment here");
        return;
    }

    if (findFold(threadFolds, id).view == ThreadView::collapsed)
        setThreadView(id, ThreadView::recent);

    editingId = id;
    replyCommentId.reset();
    pendingAnnotation.reset();
    openEditor("Edit comment · " + existing->path + ":" + lineLabel(*existing),
               existing->text);
}

void Viewer::removeImages(const Annotation& note)
{
    auto texts = std::vector<std::string> {note.text};

    for (const auto& reply: note.replies)
        texts.push_back(reply.text);

    for (const auto& text: texts)
    {
        for (const auto& path: imagesIn(text))
            eacp::Files::removeFile(store.home / path);
    }
}

void Viewer::deleteAtCursor()
{
    auto id = annotationAtCursor();
    auto* existing = annotation(id);

    if (existing == nullptr)
    {
        say("No comment here");
        return;
    }

    // The agent already has a sent one, so it is withdrawn instead.
    if (existing->round.empty())
    {
        removeImages(*existing);
        std::erase_if(annotations, [&](const Annotation& a) { return a.id == id; });
        say("Comment deleted");
    }
    else
    {
        existing->withdrawn = true;
        existing->draft = true;
        say("Withdrawn: the next round tells the agent");
    }

    saveStore();
    rebuildRows();
}

void Viewer::toggleResolvedAtCursor()
{
    auto* existing = annotation(annotationAtCursor());

    if (existing == nullptr)
    {
        say("No comment here");
        return;
    }

    existing->resolved = !existing->resolved;
    existing->draft = true;
    say(existing->resolved ? "Resolved" : "Reopened");
    saveStore();
    rebuildRows();
}

void Viewer::replyAtCursor()
{
    auto id = annotationAtCursor();
    auto* note = annotation(id);

    if (note == nullptr)
    {
        say("No comment here");
        return;
    }

    if (findFold(threadFolds, id).view == ThreadView::collapsed)
        setThreadView(id, ThreadView::recent);

    auto pending = findPendingReply(*note);
    replyCommentId = id;
    editingId = 0;
    pendingAnnotation.reset();
    openEditor((pending ? "Edit reply · #" : "Reply · #") + std::to_string(id),
               pending ? note->replies[*pending].text : std::string {});
}

bool Viewer::completeMention()
{
    const auto& line = editor.lines()[static_cast<std::size_t>(editor.row())];
    auto end = static_cast<std::size_t>(editor.column());
    auto start = end;

    auto isNameChar = [](char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'
               || c == '.';
    };

    while (start > 0 && isNameChar(line[start - 1]))
        --start;

    if (start == 0 || line[start - 1] != '@')
        return false;

    auto typed = line.substr(start, end - start);
    auto names = std::set<std::string> {listening.begin(), listening.end()};

    for (const auto& note: annotations)
    {
        if (auto owner = findOwner(note); !owner.empty())
            names.insert(owner);

        for (const auto& reply: note.replies)
        {
            if (reply.author != reviewerName && !reply.author.empty())
                names.insert(reply.author);
        }
    }

    auto matches = std::vector<std::string> {};

    for (const auto& name: names)
    {
        if (name.starts_with(typed))
            matches.push_back(name);
    }

    if (matches.empty())
    {
        if (listening.empty())
            say("No agent is listening (dv monitor --as <name>)");
        else
            say("No agent @" + typed + "…");

        return true;
    }

    auto common = matches.front();

    for (const auto& name: matches)
    {
        while (!name.starts_with(common))
            common.pop_back();
    }

    editor.insertText(common.substr(typed.size())
                      + (matches.size() == 1 ? " " : ""));

    if (matches.size() > 1)
    {
        auto list = std::string {};

        for (const auto& name: matches)
            list += "@" + name + " ";

        say(list);
    }

    return true;
}

void Viewer::clearResolved()
{
    showResolved = !showResolved;
    rebuildRows();
    say(showResolved ? "Showing resolved comments" : "Hiding resolved comments");
}
} // namespace diffview

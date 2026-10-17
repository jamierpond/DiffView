#include "Clipboard.h"
#include "Viewer.h"

#include <Text.h>

#include <eacp/Core/App/Clipboard.h>
#include <eacp/Core/Utils/Files.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace diffview
{
namespace
{
std::string readFile(const std::filesystem::path& path)
{
    auto in = std::ifstream {path, std::ios::binary};
    auto buffer = std::stringstream {};
    buffer << in.rdbuf();
    return buffer.str();
}

void writeFile(const std::filesystem::path& path, const std::string& text)
{
    auto bytes =
        std::span {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
    eacp::Files::writeFileAtomically(path.string(), bytes);
}

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
    auto signature = std::string {};
    auto dir = std::filesystem::path {repo} / reviewDirectory;
    auto error = std::error_code {};

    if (!std::filesystem::is_directory(dir, error))
        return signature;

    auto names = std::vector<std::string> {};

    for (const auto& entry: std::filesystem::directory_iterator {dir, error})
        if (entry.path().extension() == ".md")
            names.push_back(
                entry.path().filename().string() + ":"
                + std::to_string(entry.file_size(error)) + ":"
                + std::to_string(static_cast<long long>(
                    entry.last_write_time(error).time_since_epoch().count())));

    std::sort(names.begin(), names.end());

    for (const auto& name: names)
        signature += name + "\n";

    auto latest = std::filesystem::path {repo} / latestReviewFileName;

    if (std::filesystem::exists(latest, error))
        signature += readFile(latest);

    return signature;
}

void Viewer::loadStore()
{
    auto root = std::filesystem::path {repo};
    auto roundFiles = std::vector<ReviewFile> {};
    auto error = std::error_code {};
    auto names = std::vector<std::filesystem::path> {};

    if (std::filesystem::is_directory(root / reviewDirectory, error))
        for (const auto& entry:
             std::filesystem::directory_iterator {root / reviewDirectory, error})
            if (entry.path().extension() == ".md")
                names.push_back(entry.path());

    std::sort(names.begin(), names.end());

    for (const auto& path: names)
    {
        auto file = fromMarkdown(readFile(path));
        file.title = path.stem().string();
        roundFiles.push_back(std::move(file));
    }

    auto latestPath = root / latestReviewFileName;

    if (!roundFiles.empty() && std::filesystem::exists(latestPath))
        if (auto resolved = resolutionsFrom(fromMarkdown(readFile(latestPath)),
                                            roundFiles.back()))
        {
            writeFile(root / reviewDirectory / (resolved->title + ".md"),
                      toMarkdown(*resolved));
            roundFiles.back() = std::move(*resolved);
        }

    auto draftPath = root / draftFileName;
    lastDraftText =
        std::filesystem::exists(draftPath) ? readFile(draftPath) : std::string {};
    auto merged = mergeReviews(roundFiles, fromMarkdown(lastDraftText));

    annotations = std::move(merged.annotations);
    rounds = std::move(merged.rounds);
    lastRoundsSignature = roundsSignature();

    if (merged.renumbered)
        saveStore();
}

bool Viewer::reloadStoreIfChanged()
{
    auto draftPath = std::filesystem::path {repo} / draftFileName;
    auto draftText =
        std::filesystem::exists(draftPath) ? readFile(draftPath) : std::string {};

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

    auto dir = std::filesystem::path {repo} / annotationDirectory;

    if (draftCount(annotations) == 0 && !std::filesystem::exists(dir))
        return;

    std::filesystem::create_directories(dir);

    // Ignores itself, so an agent's `git add -A` never sweeps it up.
    if (!std::filesystem::exists(dir / ".gitignore"))
        std::ofstream {dir / ".gitignore"} << "*\n";

    writeFile(std::filesystem::path {repo} / draftFileName, text);
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

    auto root = std::filesystem::path {repo};
    auto id = roundId(nowMs());

    for (auto n = 2; std::filesystem::exists(root / reviewDirectory / (id + ".md"));
         ++n)
        id = roundId(nowMs()) + "-" + std::to_string(n);

    auto round = submitDraft(annotations, id, base.label(), baseSha);
    auto review = toMarkdown(round);

    std::filesystem::create_directories(root / reviewDirectory);
    writeFile(root / reviewDirectory / (id + ".md"), review);

    writeFile(root / latestReviewFileName, review);

    rounds.push_back(id);
    lastRoundsSignature = roundsSignature();
    saveStore();
    rebuildRows();
    say("Submitted " + std::to_string(round.comments.size())
        + " comments → .diffview/latest.md");
}

void Viewer::annotateAtCursor(bool wholeHunk)
{
    const auto* file = selectedFile();

    if (file == nullptr || rows.empty())
    {
        say("Nothing to annotate");
        return;
    }

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

    pendingAnnotation =
        makeAnnotation(*file, *range, nextAnnotationId(annotations), {});
    editingId = 0;
    openEditor("New comment · " + file->path + ":" + lineLabel(*pendingAnnotation),
               {});
}

void Viewer::commentOnFile()
{
    const auto* file = selectedFile();

    if (file == nullptr || file->path.empty())
    {
        commentGeneral();
        return;
    }

    visualAnchor.reset();
    pendingAnnotation =
        makeFileAnnotation(file->path, nextAnnotationId(annotations), {});
    editingId = 0;
    diffCursor = 0;
    openEditor("New comment · " + file->path + " (whole file)", {});
}

void Viewer::commentGeneral()
{
    visualAnchor.reset();
    pendingAnnotation = makeFileAnnotation({}, nextAnnotationId(annotations), {});
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

    switch (editor.feed(key))
    {
        case vim::Outcome::none:
            break;
        case vim::Outcome::save:
            if (commitEditor())
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
            if (std::find(linked.begin(), linked.end(), path) == linked.end())
            {
                auto error = std::error_code {};
                std::filesystem::remove(std::filesystem::path {repo} / path, error);
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
    auto linked =
        saved != nullptr ? imagesIn(saved->text) : std::vector<std::string> {};

    for (const auto& path: pastedImages)
        if (std::find(linked.begin(), linked.end(), path) == linked.end())
        {
            auto error = std::error_code {};
            std::filesystem::remove(std::filesystem::path {repo} / path, error);
        }

    auto id = editingId;
    editing = false;
    editingId = 0;
    pendingAnnotation.reset();
    pastedImages.clear();
    rebuildRows();

    if (id != 0)
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].kind == RowKind::annotationTop && rows[i].annotation == id)
            {
                diffCursor = static_cast<int>(i);
                break;
            }
}

void Viewer::pasteIntoEditor()
{
    if (auto png = clipboard::imagePng())
    {
        auto id = editingId != 0      ? editingId
                  : pendingAnnotation ? pendingAnnotation->id
                                      : nextAnnotationId(annotations);
        auto dir = std::filesystem::path {repo} / imageDirectory;
        std::filesystem::create_directories(dir);

        auto name = std::string {};

        for (auto n = 1;; ++n)
        {
            name = std::to_string(id) + "-" + std::to_string(n) + ".png";

            if (!std::filesystem::exists(dir / name))
                break;
        }

        writeFile(dir / name, *png);

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

    editingId = id;
    pendingAnnotation.reset();
    openEditor("Edit comment · " + existing->path + ":" + lineLabel(*existing),
               existing->text);
}

void Viewer::removeImages(const Annotation& note)
{
    for (const auto& path: imagesIn(note.text))
    {
        auto error = std::error_code {};
        std::filesystem::remove(std::filesystem::path {repo} / path, error);
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

void Viewer::clearResolved()
{
    showResolved = !showResolved;
    rebuildRows();
    say(showResolved ? "Showing resolved comments" : "Hiding resolved comments");
}
} // namespace diffview

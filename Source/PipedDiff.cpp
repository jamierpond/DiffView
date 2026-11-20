#include "Agents.h"
#include "Viewer.h"

namespace diffview
{
bool Viewer::isShowingPipedDiff() const
{
    return options.pipedFiles.has_value();
}

bool Viewer::refuseForPipedDiff(std::string_view feature)
{
    if (!isShowingPipedDiff())
        return false;

    say(std::string {feature} + " isn't available for a piped diff");
    return true;
}

bool Viewer::refuseCommentWhenReadOnly()
{
    if (!options.isReadOnly)
        return false;

    say("Read-only: comments live in a git work tree's .diffview, and this "
        "isn't inside one");
    return true;
}

std::string Viewer::describeBase() const
{
    if (isShowingPipedDiff())
        return "standard input";

    return base.label();
}

void Viewer::reloadReviewStoreUnlessReadOnly()
{
    if (options.isReadOnly)
        return;

    reloadStoreIfChanged();
    listening = findListeningAgents(store.directory.str());
}
} // namespace diffview

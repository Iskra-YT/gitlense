#include "commands/command.h"

#include "analysis/history.h"
#include "commands/command_list.h"
#include "core/error.h"
#include "core/walker.h"
#include "format/color.h"
#include "format/table.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace gitlense {

Registry& Registry::instance()
{
    static Registry registry;
    return registry;
}

void Registry::add(Command command)
{
    commands_.push_back(std::move(command));
}

const Command* Registry::find(const std::string& name) const
{
    for (const Command& command : commands_) {
        if (name == command.name) {
            return &command;
        }
    }
    // obsluga unikalnych prefiksow ("contrib" -> "contributors")
    const Command* partial = nullptr;
    for (const Command& command : commands_) {
        if (std::string(command.name).rfind(name, 0) == 0) {
            if (partial != nullptr) {
                return nullptr; // niejednoznaczne
            }
            partial = &command;
        }
    }
    return partial;
}

std::vector<const Command*> Registry::sorted() const
{
    std::vector<const Command*> result;
    result.reserve(commands_.size());
    for (const Command& command : commands_) {
        result.push_back(&command);
    }
    std::sort(result.begin(), result.end(),
              [](const Command* a, const Command* b) { return std::string(a->name) < std::string(b->name); });
    return result;
}

std::string Registry::overview() const
{
    Table table({{"KOMENDA", Align::Left}, {"OPIS", Align::Left}});
    for (const Command* command : sorted()) {
        table.addRow({Style::boldColor("cyan").wrap(command->name), command->summary});
    }
    return table.render();
}

std::string Registry::helpFor(const Command& command)
{
    std::string out;
    out += Style::bold().wrap("gitlense " + std::string(command.name)) + " - "
        + command.summary + "\n\n";
    if (command.details != nullptr && *command.details != '\0') {
        out += command.details;
        if (command.details[std::strlen(command.details) - 1] != '\n') {
            out += "\n";
        }
        out += "\n";
    }
    if (command.examples != nullptr && *command.examples != '\0') {
        out += Style::bold().wrap("Przyklady") + "\n" + command.examples + "\n";
    }
    out += "\n"
        + Style::dim().wrap("Opcje globalne: --repo DIR, --threads N, --date=STYL, "
                            "--path-width N, --no-color, --quiet, --no-header, --help\n");
    return out;
}

// ---------------------------------------------------------------------------
// Helpery
// ---------------------------------------------------------------------------

WalkOptions walkOptionsFromArgs(const Arguments& args)
{
    WalkOptions options;
    options.limit = args.getSize("limit", args.getSize("n", 0));
    if (args.has("since")) {
        options.since = parseTimestamp(args.get("since"));
    }
    if (args.has("until")) {
        options.until = parseTimestamp(args.get("until"), true);
    }
    options.authorPattern = args.get("author", "");
    options.committerPattern = args.get("committer", "");
    options.messagePattern = args.get("grep", "");
    options.pathPattern = args.get("path", "");
    options.includeAllRefs = args.flag("all");
    options.firstParentOnly = args.flag("first-parent");
    options.reverse = args.flag("reverse");
    options.includeMergeCommits = !args.flag("no-merges");

    if (args.flag("last") && options.limit == 0) {
        options.limit = 1;
    }
    for (const std::string& value : args.getAll("exclude")) {
        options.excludePaths.push_back(value);
    }
    return options;
}

PathFilter pathFilterFromArgs(const Arguments& args)
{
    std::vector<std::string> include;
    for (const std::string& value : args.getAll("include")) {
        include.push_back(value);
    }
    if (const std::string path = args.get("path", ""); !path.empty()) {
        include.push_back(path);
    }
    std::vector<std::string> exclude;
    for (const std::string& value : args.getAll("exclude")) {
        exclude.push_back(value);
    }
    return PathFilter(std::move(include), std::move(exclude));
}

void printRepoHeader(const CommandContext& ctx, const std::string& title,
                     const std::string& subtitle)
{
    if (ctx.quiet) {
        return;
    }
    const Repository& repo = ctx.repo;
    std::string line = Style::bold().wrap(title);

    std::vector<std::string> facts;
    const std::string name = repo.location().workdir.filename().string();
    if (!name.empty()) {
        facts.push_back(name);
    }
    facts.push_back("branch: " + repo.headDescription());
    const Oid head = repo.headOrNull();
    if (!isNull(head)) {
        facts.push_back(toShortHex(head));
    }
    if (!repo.isBare() && repo.workingTreeChanges() > 0) {
        facts.push_back(Style::yellow().wrap("brudne drzewo: "
                                             + std::to_string(repo.workingTreeChanges())
                                             + " zmian"));
    }
    line += "  " + Style::dim().wrap([&] {
        std::string out;
        for (std::size_t i = 0; i < facts.size(); ++i) {
            out += (i != 0 ? " | " : "") + facts[i];
        }
        return out;
    }());
    std::cout << line << "\n";
    if (!subtitle.empty()) {
        std::cout << Style::dim().wrap(subtitle) << "\n";
    }
    std::cout << "\n";
}

std::string shortenPath(const CommandContext& ctx, const std::string& path)
{
    return middleEllipsize(path, ctx.pathWidth);
}

void registerAllCommands()
{
    Registry& registry = Registry::instance();
    registry.add(logCommand);
    registry.add(showCommand);
    registry.add(fileCommand);
    registry.add(branchesCommand);
    registry.add(diffCommand);
    registry.add(contributorsCommand);
    registry.add(languagesCommand);
    registry.add(hotspotsCommand);
    registry.add(activityCommand);
    registry.add(shortlogCommand);
    registry.add(searchCommand);
    registry.add(graphCommand);
}

} // namespace gitlense

#include "analysis/activity.h"

#include "core/error.h"
#include "format/format.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <set>
#include <unordered_map>

namespace gitlense {
namespace {

Timestamp bucketStart(Timestamp ts, Granularity granularity)
{
    switch (granularity) {
    case Granularity::Day:
        return startOfDay(ts);
    case Granularity::Week:
        return startOfWeek(ts);
    case Granularity::Month:
        return startOfMonth(ts);
    case Granularity::Year:
        return startOfYear(ts);
    }
    return ts;
}

} // namespace

std::string bucketLabel(Timestamp ts, Granularity granularity)
{
    switch (granularity) {
    case Granularity::Day: {
        std::tm tm{};
        const std::time_t raw = static_cast<std::time_t>(ts);
        ::localtime_r(&raw, &tm);
        return monthLabel(ts) + " " + std::to_string(tm.tm_mday);
    }
    case Granularity::Week:
        return "tydz. " + monthLabel(ts);
    case Granularity::Month:
        return monthLabel(ts);
    case Granularity::Year:
        return yearLabel(ts);
    }
    return {};
}

Granularity parseGranularity(const std::string& value)
{
    if (value == "day" || value == "d" || value == "dzien") {
        return Granularity::Day;
    }
    if (value == "week" || value == "w" || value == "tydzien") {
        return Granularity::Week;
    }
    if (value == "month" || value == "m" || value == "miesiac") {
        return Granularity::Month;
    }
    if (value == "year" || value == "y" || value == "rok") {
        return Granularity::Year;
    }
    throw UsageError("nieznana granulacja \"" + value + "\" (uzyj: day, week, month, year)");
}

const char* toString(Granularity granularity)
{
    switch (granularity) {
    case Granularity::Day:
        return "dzien";
    case Granularity::Week:
        return "tydzien";
    case Granularity::Month:
        return "miesiac";
    case Granularity::Year:
        return "rok";
    }
    return "miesiac";
}

std::int64_t stepOf(Granularity granularity)
{
    switch (granularity) {
    case Granularity::Day:
        return 86400;
    case Granularity::Week:
        return 7 * 86400;
    case Granularity::Month:
        return 30 * 86400;
    case Granularity::Year:
        return 365 * 86400;
    }
    return 30 * 86400;
}

ActivityResult analyzeActivity(const Repository& repo, const LanguageDetector& detector,
                               const ActivityOptions& options)
{
    ActivityResult result;
    result.granularity = options.granularity;

    const Oid start = options.walk.includeAllRefs ? kNullOid : repo.headOrNull();
    if (isNull(start) && !options.walk.includeAllRefs) {
        return result;
    }

    HistoryOptions historyOptions;
    historyOptions.walk = options.walk;
    HistoryScan scan(repo, historyOptions, detector);
    const std::vector<CommitChange> history = scan.collect(options.threads);
    if (history.empty()) {
        return result;
    }

    std::map<Timestamp, ActivityBucket> buckets;
    std::map<std::string, AuthorActivity> authors;

    for (const CommitChange& change : history) {
        std::set<std::string> filesInCommit;
        std::uint64_t churn = 0;
        std::uint64_t additions = 0;
        std::uint64_t deletions = 0;
        for (const FileChangeRecord& file : change.files) {
            if (!options.path.accepts(file.path)) {
                continue;
            }
            filesInCommit.insert(file.path);
            churn += file.churn();
            additions += file.additions;
            deletions += file.deletions;
        }

        const Timestamp bucket = bucketStart(change.time, options.granularity);
        ActivityBucket& target = buckets[bucket];
        target.start = bucket;
        target.label = bucketLabel(bucket, options.granularity);
        target.commits += 1;
        target.additions += additions;
        target.deletions += deletions;
        target.authorNames.insert(change.authorName);
        target.authors = target.authorNames.size();
        target.files.insert(filesInCommit.begin(), filesInCommit.end());

        AuthorActivity& author = authors[change.authorName];
        author.author = change.authorName;
        author.commits += 1;
        author.churn += churn;
        author.firstCommit = author.firstCommit == 0
            ? change.time
            : std::min(author.firstCommit, change.time);
        author.lastCommit = std::max(author.lastCommit, change.time);

        const std::time_t raw = static_cast<std::time_t>(change.time);
        std::tm tm{};
        ::localtime_r(&raw, &tm);
        result.commitsOnWeekdays[tm.tm_wday % 7] += 1;
        result.commitsOnHours[tm.tm_hour % 24] += 1;
        result.activeDays.push_back(startOfDay(change.time));
        result.totalCommits += 1;
        result.totalChurn += churn;
        result.additionsTotal += additions;
        result.deletionsTotal += deletions;
        result.firstCommit = result.firstCommit == 0
            ? change.time
            : std::min(result.firstCommit, change.time);
        result.lastCommit = std::max(result.lastCommit, change.time);
    }

    std::sort(result.activeDays.begin(), result.activeDays.end());
    result.activeDays.erase(std::unique(result.activeDays.begin(), result.activeDays.end()),
                            result.activeDays.end());

    // serie aktywnych dni (kolejne dni = 86400 sekund; dopuszczamy 25h,
    // zeby uwzglednic strefy czasowe)
    std::size_t streak = 0;
    Timestamp previous = 0;
    for (const Timestamp day : result.activeDays) {
        if (previous != 0 && day - previous <= 25 * 3600) {
            streak += 1;
        } else {
            streak = 1;
        }
        result.longestStreak = std::max(result.longestStreak, streak);
        previous = day;
    }
    result.currentStreak = 0;
    if (!result.activeDays.empty()
        && (now() - result.activeDays.back()) / 86400 < 7) {
        result.currentStreak = streak;
    }
    for (auto& entry : authors) {
        AuthorActivity& author = entry.second;
        author.longestStreak = result.longestStreak;
        author.currentStreak = result.currentStreak;
    }

    for (auto& [startTime, bucket] : buckets) {
        (void)startTime;
        result.buckets.push_back(std::move(bucket));
    }
    std::sort(result.buckets.begin(), result.buckets.end(),
              [](const ActivityBucket& a, const ActivityBucket& b) { return a.start < b.start; });
    if (options.maxBuckets != 0 && result.buckets.size() > options.maxBuckets) {
        result.buckets.erase(result.buckets.begin(),
                             result.buckets.end() - static_cast<std::ptrdiff_t>(options.maxBuckets));
    }

    // trendy: srednia zmiana liczby commitow na okres
    if (result.buckets.size() >= 2) {
        const double first = static_cast<double>(result.buckets.front().commits);
        const double last = static_cast<double>(result.buckets.back().commits);
        result.trendPerPeriod = (last - first) / static_cast<double>(result.buckets.size() - 1);
    }

    // aktywnosc autorow w kolejnych okresach
    std::map<std::pair<std::string, Timestamp>, std::size_t> perAuthorPeriod;
    for (const CommitChange& change : history) {
        perAuthorPeriod[{change.authorName, bucketStart(change.time, options.granularity)}] += 1;
    }
    for (auto& [name, author] : authors) {
        (void)name;
        for (const ActivityBucket& bucket : result.buckets) {
            const auto it = perAuthorPeriod.find({author.author, bucket.start});
            if (it != perAuthorPeriod.end()) {
                author.perPeriod.emplace_back(bucket.start, it->second);
            }
        }
        const double weeks = static_cast<double>(author.lastCommit - author.firstCommit)
            / (7.0 * 86400.0);
        author.commitsPerWeek = weeks <= 0
            ? static_cast<double>(author.commits)
            : static_cast<double>(author.commits) / (weeks + 1.0);
    }

    for (auto& [name, author] : authors) {
        (void)name;
        result.authors.push_back(std::move(author));
    }
    std::sort(result.authors.begin(), result.authors.end(),
              [](const AuthorActivity& a, const AuthorActivity& b) {
                  if (a.commits != b.commits) {
                      return a.commits > b.commits;
                  }
                  return a.author < b.author;
              });

    return result;
}

} // namespace gitlense

#include <userver/utils/statistics/testing.hpp>

#include <algorithm>
#include <unordered_map>
#include <utility>

#include <fmt/format.h>
#include <boost/algorithm/cxx11/all_of.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/iterator_range.hpp>

#include <userver/utils/statistics/fmt.hpp>

USERVER_NAMESPACE_BEGIN

namespace utils::statistics {
namespace {

struct Metric final {
  std::string path;
  std::vector<Label> labels;
  MetricValue value;
};

std::string ToString(const Metric& metric) {
  return fmt::format("{};{} {}", metric.path, fmt::join(metric.labels, ";"),
                     metric.value);
}

struct SnapshotDataEntry final {
  boost::container::flat_set<Label> labels;
  MetricValue value;
};

}  // namespace
namespace impl {

struct SnapshotData final {
  std::unordered_multimap<std::string, SnapshotDataEntry> metrics;
};

}  // namespace impl
namespace {

class SnapshotVisitor final : public BaseFormatBuilder {
 public:
  explicit SnapshotVisitor(impl::SnapshotData& data) : data_(data) {}

  void HandleMetric(std::string_view path, LabelsSpan labels,
                    const MetricValue& value) override {
    auto labels_copy =
        labels | boost::adaptors::transformed([&](auto label) {
          return Label{std::string{label.Name()}, std::string{label.Value()}};
        });
    SnapshotDataEntry entry{{labels_copy.begin(), labels_copy.end()}, value};
    data_.metrics.emplace(std::string{path}, std::move(entry));
  }

 private:
  impl::SnapshotData& data_;
};

utils::SharedRef<const impl::SnapshotData> BuildSnapshotData(
    const Storage& storage, const StatisticsRequest& request) {
  auto data = utils::MakeSharedRef<impl::SnapshotData>();
  SnapshotVisitor visitor{*data};
  storage.VisitMetrics(visitor, request);
  return std::move(data);
}

void PrependPrefix(std::string& path, const StatisticsRequest& request) {
  const std::string_view separator =
      (path.empty() || request.prefix.empty()) ? "" : ".";
  path = fmt::format("{}{}{}", request.prefix, separator, path);
}

Metric GetSingle(const impl::SnapshotData& data, const std::string& path,
                 const std::vector<Label>& required_labels) {
  std::optional<Metric> found_metric;
  const auto iterator_pair = data.metrics.equal_range(path);

  for (const auto& [_, entry] : boost::make_iterator_range(iterator_pair)) {
    const bool matches = boost::algorithm::all_of(
        required_labels, [& entry = entry](const auto& needle) {
          return entry.labels.count(needle) != 0;
        });

    if (matches) {
      Metric new_metric{
          path, {entry.labels.begin(), entry.labels.end()}, entry.value};
      if (found_metric) {
        throw MetricQueryError(
            fmt::format("Multiple metrics found for request {};{}\n  {}\n  {}",
                        path, fmt::join(required_labels, ";"),
                        ToString(*found_metric), ToString(new_metric)));
      }
      found_metric = std::move(new_metric);
    }
  }

  if (!found_metric) {
    throw MetricQueryError(fmt::format("No metric found for request {};{}",
                                       path, fmt::join(required_labels, ";")));
  }
  return std::move(*found_metric);
}

}  // namespace

Snapshot::Snapshot(const Storage& storage, std::string prefix,
                   std::vector<Label> require_labels)
    : request_(StatisticsRequest::MakeWithPrefix(std::move(prefix), {},
                                                 std::move(require_labels))),
      data_(BuildSnapshotData(storage, request_)) {}

MetricValue Snapshot::SingleMetric(std::string path,
                                   std::vector<Label> require_labels) const {
  PrependPrefix(path, request_);
  auto result = statistics::GetSingle(*data_, path, require_labels);
  return result.value;
}

}  // namespace utils::statistics

USERVER_NAMESPACE_END

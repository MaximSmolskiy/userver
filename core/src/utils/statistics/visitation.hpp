#pragma once

#include <string>
#include <variant>
#include <vector>

#include <userver/formats/json_fwd.hpp>

USERVER_NAMESPACE_BEGIN

namespace utils::statistics {

class BaseFormatBuilder;
class StatisticsRequest;

void VisitMetrics(BaseFormatBuilder& out,
                  const formats::json::Value& statistics_storage_json,
                  const StatisticsRequest& request);

}  // namespace utils::statistics

USERVER_NAMESPACE_END

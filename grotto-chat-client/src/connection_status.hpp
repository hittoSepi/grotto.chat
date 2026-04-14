#pragma once

#include <string>
#include <string_view>

namespace grotto {

std::string sanitize_disconnect_reason(std::string_view reason);
std::string connection_summary_for_phase(std::string_view phase);

} // namespace grotto

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal {

enum class ListNumberFormat {
  none,
  bullet,
  decimal,
  decimal_zero,
  letter_lower,
  letter_upper,
  roman_lower,
  roman_upper,
};

/// @brief One level of a list style, in the shape ODF and OOXML both lower to.
///
/// `label` is OOXML's `w:lvlText`: literal text in which `%N` stands for the
/// counter of level N, counted from 1. ODF has no such template — its
/// prefix / suffix / `text:display-levels` compose into one, and as it has a
/// tenth level, `%{N}` stands for level N too.
struct ListLevel final {
  ListNumberFormat format{ListNumberFormat::none};
  std::string label;
  std::uint32_t start{1};
};

/// A resolved list item label.
struct ListMarker final {
  std::string text;
  std::optional<std::uint32_t> number;
};

std::string format_list_number(ListNumberFormat format, std::uint32_t number);

/// A bullet character in Unicode. Word sets its bullets in Symbol or Wingdings
/// at private-use code points U+F020 to U+F0FF. In any other font they show as
/// a box. @p font_family is the family the bullet is set in, or empty if it is
/// unknown. Any font other than Symbol is read as Wingdings. A private-use
/// character without a mapping, or an empty or malformed @p text, gives a
/// default for @p level (counted from 0).
std::string resolve_bullet(const std::string &text, std::uint32_t level,
                           std::string_view font_family = {});

/// The counters of one list, indexed by level.
class ListCounter final {
public:
  /// Advances `level` (counted from 0), resets the levels below it, and expands
  /// that level's label against the resulting counters.
  std::string advance(std::uint32_t level, const ListLevel &list_level);

  /// The counter `advance` last produced for `level`.
  [[nodiscard]] std::uint32_t number(std::uint32_t level) const;

  void restart(std::uint32_t level, std::uint32_t number);

  /// Opens `level` at its start, unless it is open already, without counting
  /// an item on it.
  void start(std::uint32_t level, const ListLevel &list_level);

private:
  std::vector<std::optional<std::uint32_t>> m_numbers;
  std::vector<ListNumberFormat> m_formats;

  void grow_(std::uint32_t level);
};

} // namespace odr::internal

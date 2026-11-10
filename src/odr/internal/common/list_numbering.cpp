#include <odr/internal/common/list_numbering.hpp>

#include <odr/internal/util/number_util.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace odr::internal {

namespace {

std::string format_decimal(const std::uint32_t number) {
  return std::to_string(number);
}

std::string format_letter(std::uint32_t number, const char base) {
  std::string result;
  for (; number > 0; number = (number - 1) / 26) {
    result.push_back(static_cast<char>(base + (number - 1) % 26));
  }
  std::ranges::reverse(result);
  return result;
}

std::string format_roman(std::uint32_t number, const bool upper) {
  static constexpr std::array<std::uint32_t, 13> values{
      1000, 900, 500, 400, 100, 90, 50, 40, 10, 9, 5, 4, 1};
  static constexpr std::array<const char *, 13> upper_symbols{
      "M", "CM", "D", "CD", "C", "XC", "L", "XL", "X", "IX", "V", "IV", "I"};
  static constexpr std::array<const char *, 13> lower_symbols{
      "m", "cm", "d", "cd", "c", "xc", "l", "xl", "x", "ix", "v", "iv", "i"};

  // Roman has no zero and the additive notation runs out of thousands marks.
  if (number == 0 || number > 3999) {
    return format_decimal(number);
  }

  const auto &symbols = upper ? upper_symbols : lower_symbols;

  std::string result;
  for (std::size_t i = 0; i < values.size(); ++i) {
    for (; number >= values[i]; number -= values[i]) {
      result.append(symbols[i]);
    }
  }
  return result;
}

/// The Symbol font's characters that lists use, by their byte, in Unicode
/// after Adobe's `symbol.txt`, which leaves the horizontal line 0xBE in its
/// private use area.
constexpr std::array<std::pair<char32_t, std::string_view>, 11>
    symbol_characters{{
        {0x2A, "∗"},
        {0x2D, "−"},
        {0x3E, ">"},
        {0xA7, "♣"},
        {0xA8, "♦"},
        {0xAE, "→"},
        {0xB7, "•"},
        {0xBE, "⎯"},
        {0xD8, "¬"},
        {0xDE, "⇒"},
        {0xE0, "◊"},
    }};

/// The Wingdings font's characters that lists use, by their byte, in Unicode.
constexpr std::array<std::pair<char32_t, std::string_view>, 14>
    wingdings_characters{{
        {0x6C, "●"},
        {0x6E, "■"},
        {0x6F, "□"},
        {0x71, "❑"},
        {0x75, "◆"},
        {0x76, "❖"},
        {0x77, "⬥"},
        {0xA7, "▪"},
        {0xA8, "◻"},
        {0xD8, "➢"},
        {0xF0, "⇨"},
        {0xFC, "✓"},
        {0xFD, "☒"},
        {0xFE, "☑"},
    }};

/// The Zapf Dingbats font's characters that lists use, by their byte, in
/// Unicode, whose Dingbats block follows the font.
constexpr std::array<std::pair<char32_t, std::string_view>, 6>
    zapf_dingbats_characters{{
        {0x6C, "●"},
        {0x6E, "■"},
        {0x6F, "❏"},
        {0x71, "❑"},
        {0x75, "◆"},
        {0x76, "❖"},
    }};

/// A symbol font's characters by their byte.
using CharacterTable = std::span<const std::pair<char32_t, std::string_view>>;

/// The table for @p font_family, which comes quoted from `svg:font-family`,
/// or empty for a font that encodes its characters as Unicode.
CharacterTable symbol_font_table(std::string_view font_family) {
  font_family = util::string::trim_view(font_family, [](const char c) {
    return c == '\'' || c == '"' || util::string::is_ascii_whitespace(c);
  });
  if (util::string::equals_ignore_case(font_family, "Symbol")) {
    return symbol_characters;
  }
  if (util::string::equals_ignore_case(font_family, "Wingdings")) {
    return wingdings_characters;
  }
  if (util::string::equals_ignore_case(font_family, "ZapfDingbats") ||
      util::string::equals_ignore_case(font_family, "Zapf Dingbats") ||
      util::string::equals_ignore_case(font_family, "ITC Zapf Dingbats")) {
    return zapf_dingbats_characters;
  }
  return {};
}

/// The byte a symbol font's @p character stands for, stated as the byte or at
/// U+F0xx, or 0 for any other character.
char32_t symbol_font_byte(const char32_t character) {
  if (character >= 0xF020 && character <= 0xF0FF) {
    return character - 0xF000;
  }
  if (character >= 0x20 && character <= 0xFF) {
    return character;
  }
  return 0;
}

/// The bullet of @p level (counted from 0) where the stated one cannot show.
std::string default_bullet(const std::uint32_t level) {
  static constexpr std::array<const char *, 3> defaults{"•", "◦", "▪"};
  return defaults[level % defaults.size()];
}

} // namespace

} // namespace odr::internal

namespace odr {

std::string internal::format_list_number(const ListNumberFormat format,
                                         const std::uint32_t number) {
  switch (format) {
  case ListNumberFormat::decimal:
    return format_decimal(number);
  case ListNumberFormat::decimal_zero:
    return number < 10 ? "0" + format_decimal(number) : format_decimal(number);
  case ListNumberFormat::letter_lower:
    return format_letter(number, 'a');
  case ListNumberFormat::letter_upper:
    return format_letter(number, 'A');
  case ListNumberFormat::roman_lower:
    return format_roman(number, false);
  case ListNumberFormat::roman_upper:
    return format_roman(number, true);
  case ListNumberFormat::none:
  case ListNumberFormat::bullet:
  default:
    return "";
  }
}

std::string internal::resolve_bullet(const std::string &text,
                                     const std::uint32_t level,
                                     const std::string_view font_family) {
  if (text.empty() || !util::string::is_valid_utf8(text)) {
    return default_bullet(level);
  }

  std::string_view remaining = text;
  const char32_t first = util::string::next_utf8(remaining);
  const CharacterTable table = symbol_font_table(font_family);
  const char32_t byte = symbol_font_byte(first);
  if (table.empty() || byte == 0) {
    // a private-use character means nothing outside its font
    return first >= 0xE000 && first <= 0xF8FF ? default_bullet(level) : text;
  }
  const auto it = std::ranges::find(
      table, byte, &std::pair<char32_t, std::string_view>::first);
  return it != std::end(table) ? std::string(it->second)
                               : default_bullet(level);
}

} // namespace odr

namespace odr::internal {

std::string ListCounter::advance(const std::uint32_t level,
                                 const ListLevel &list_level) {
  grow_(level);

  auto &number = m_numbers[level];
  if (number == std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("List counter overflow");
  }
  number = number.has_value() ? *number + 1 : list_level.start;
  m_formats[level] = list_level.format;
  std::ranges::fill(m_numbers |
                        std::views::drop(static_cast<std::size_t>(level) + 1),
                    std::nullopt);

  if (list_level.format == ListNumberFormat::bullet) {
    return list_level.label;
  }

  std::string result;
  const std::string &label = list_level.label;
  for (std::size_t i = 0; i < label.size();) {
    // `%N` with one digit, or `%{N}`
    std::uint32_t placeholder = 0;
    std::size_t length = 0;
    if (label[i] == '%' && i + 1 < label.size()) {
      if (label[i + 1] >= '1' && label[i + 1] <= '9') {
        placeholder = static_cast<std::uint32_t>(label[i + 1] - '0');
        length = 2;
      } else if (label[i + 1] == '{') {
        if (const std::size_t close = label.find('}', i + 2);
            close != std::string::npos) {
          if (const std::optional<std::uint32_t> number =
                  util::number::parse_integer<std::uint32_t>(
                      std::string_view(label).substr(i + 2, close - i - 2));
              number.has_value() && *number > 0) {
            placeholder = *number;
            length = close - i + 1;
          }
        }
      }
    }
    if (length == 0) {
      result.push_back(label[i]);
      ++i;
      continue;
    }
    if (placeholder - 1 < m_numbers.size()) {
      result.append(format_list_number(m_formats[placeholder - 1],
                                       m_numbers[placeholder - 1].value_or(1)));
    }
    i += length;
  }
  return result;
}

std::uint32_t ListCounter::number(const std::uint32_t level) const {
  return level < m_numbers.size() ? m_numbers[level].value_or(0) : 0;
}

void ListCounter::restart(const std::uint32_t level,
                          const std::uint32_t number) {
  grow_(level);
  m_numbers[level] = number == 0 ? std::nullopt : std::optional(number);
}

void ListCounter::start(const std::uint32_t level,
                        const ListLevel &list_level) {
  grow_(level);
  if (!m_numbers[level].has_value()) {
    m_numbers[level] = list_level.start;
    m_formats[level] = list_level.format;
  }
}

void ListCounter::grow_(const std::uint32_t level) {
  if (level == std::numeric_limits<std::uint32_t>::max()) {
    throw std::overflow_error("List level overflow");
  }
  if (level >= m_numbers.size()) {
    m_numbers.resize(static_cast<std::size_t>(level) + 1);
    m_formats.resize(level + 1, ListNumberFormat::decimal);
  }
}

} // namespace odr::internal

#include <odr/internal/common/list_numbering.hpp>

#include <gtest/gtest.h>

#include <limits>

using namespace odr::internal;

namespace {

ListLevel decimal_level(std::string label, const std::uint32_t start = 1) {
  return {ListNumberFormat::decimal, std::move(label), start};
}

} // namespace

TEST(FormatListNumber, decimal) {
  EXPECT_EQ("1", format_list_number(ListNumberFormat::decimal, 1));
  EXPECT_EQ("42", format_list_number(ListNumberFormat::decimal, 42));
  EXPECT_EQ("09", format_list_number(ListNumberFormat::decimal_zero, 9));
  EXPECT_EQ("10", format_list_number(ListNumberFormat::decimal_zero, 10));
}

TEST(FormatListNumber, letter) {
  EXPECT_EQ("a", format_list_number(ListNumberFormat::letter_lower, 1));
  EXPECT_EQ("z", format_list_number(ListNumberFormat::letter_lower, 26));
  EXPECT_EQ("aa", format_list_number(ListNumberFormat::letter_lower, 27));
  EXPECT_EQ("ab", format_list_number(ListNumberFormat::letter_lower, 28));
  EXPECT_EQ("AA", format_list_number(ListNumberFormat::letter_upper, 27));
}

TEST(FormatListNumber, roman) {
  EXPECT_EQ("i", format_list_number(ListNumberFormat::roman_lower, 1));
  EXPECT_EQ("iv", format_list_number(ListNumberFormat::roman_lower, 4));
  EXPECT_EQ("XIV", format_list_number(ListNumberFormat::roman_upper, 14));
  EXPECT_EQ("MCMXCIV", format_list_number(ListNumberFormat::roman_upper, 1994));
  EXPECT_EQ("4000", format_list_number(ListNumberFormat::roman_upper, 4000));
}

TEST(FormatListNumber, without_a_number) {
  EXPECT_EQ("", format_list_number(ListNumberFormat::none, 1));
  EXPECT_EQ("", format_list_number(ListNumberFormat::bullet, 1));
}

TEST(ListCounter, counts_up_within_a_level) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.");

  EXPECT_EQ("1.", counter.advance(0, level));
  EXPECT_EQ("2.", counter.advance(0, level));
  EXPECT_EQ("3.", counter.advance(0, level));
}

TEST(ListCounter, starts_at_the_level_start_value) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.", 5);

  EXPECT_EQ("5.", counter.advance(0, level));
  EXPECT_EQ("6.", counter.advance(0, level));
  counter.restart(0, 0);
  EXPECT_EQ("0.", counter.advance(0, decimal_level("%1.", 0)));
  EXPECT_EQ("1.", counter.advance(0, decimal_level("%1.", 0)));
}

TEST(ListCounter, resets_deeper_levels) {
  ListCounter counter;
  const ListLevel outer = decimal_level("%1.");
  const ListLevel inner = decimal_level("%1.%2.");

  EXPECT_EQ("1.", counter.advance(0, outer));
  EXPECT_EQ("1.1.", counter.advance(1, inner));
  EXPECT_EQ("1.2.", counter.advance(1, inner));
  EXPECT_EQ("2.", counter.advance(0, outer));
  EXPECT_EQ("2.1.", counter.advance(1, inner));
}

TEST(ListCounter, expands_each_level_in_its_own_format) {
  ListCounter counter;
  const ListLevel outer{ListNumberFormat::roman_upper, "%1.", 1};
  const ListLevel inner{ListNumberFormat::letter_lower, "%1.%2)", 1};

  EXPECT_EQ("I.", counter.advance(0, outer));
  EXPECT_EQ("I.a)", counter.advance(1, inner));
  EXPECT_EQ("I.b)", counter.advance(1, inner));
}

TEST(ListCounter, treats_a_bullet_label_as_literal) {
  ListCounter counter;
  const ListLevel level{ListNumberFormat::bullet, "%1•", 1};

  EXPECT_EQ("%1•", counter.advance(0, level));
  EXPECT_EQ("%1•", counter.advance(0, level));
}

TEST(ListCounter, restarts_where_told_to) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.");

  EXPECT_EQ("1.", counter.advance(0, level));
  EXPECT_EQ("2.", counter.advance(0, level));
  counter.restart(0, 0);
  EXPECT_EQ("1.", counter.advance(0, level));
  counter.restart(0, 9);
  EXPECT_EQ("10.", counter.advance(0, level));
}

TEST(ListCounter, keeps_a_stray_percent_literal) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1 of 100%");

  EXPECT_EQ("1 of 100%", counter.advance(0, level));
}

TEST(ListCounter, starts_deep_without_its_ancestors) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.%2.%3.");

  EXPECT_EQ("1.1.1.", counter.advance(2, level));
}

TEST(ListCounter, rejects_overflow) {
  ListCounter counter;
  constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
  EXPECT_EQ("4294967295.", counter.advance(0, decimal_level("%1.", maximum)));
  EXPECT_THROW(counter.advance(0, decimal_level("%1.")), std::overflow_error);
  EXPECT_EQ(maximum, counter.number(0));
  EXPECT_THROW(counter.advance(maximum, decimal_level("%1.")),
               std::overflow_error);
  EXPECT_THROW(counter.restart(maximum, 0), std::overflow_error);
}

TEST(ListCounter, a_braced_placeholder_names_any_level) {
  ListCounter counter;
  const ListLevel level{.format = ListNumberFormat::decimal,
                        .label = "%{1}.%{10}%{x}%{}",
                        .start = 1};
  for (std::uint32_t skipped = 0; skipped < 9; ++skipped) {
    counter.start(skipped, level);
  }
  EXPECT_EQ(counter.advance(9, level), "1.1%{x}%{}");
  // one digit after the percent sign stays one level, as in `w:lvlText`
  EXPECT_EQ(
      counter.advance(
          9, {.format = ListNumberFormat::decimal, .label = "%10", .start = 1}),
      "10");
}

TEST(ListCounter, a_started_level_counts_on_from_its_start) {
  ListCounter counter;
  const ListLevel outer{
      .format = ListNumberFormat::decimal, .label = "%1.", .start = 3};
  const ListLevel inner{
      .format = ListNumberFormat::decimal, .label = "%1.%2", .start = 1};
  counter.start(0, outer);
  EXPECT_EQ(counter.advance(1, inner), "3.1");
  EXPECT_EQ(counter.advance(0, outer), "4.");
  // an open level is not reopened
  counter.start(0, outer);
  EXPECT_EQ(counter.advance(0, outer), "5.");
}

// U+F0B7, U+F0A7 and so on: the private-use points Word writes a symbol
// font's byte at
namespace {
const std::string symbol_dot = "\xef\x82\xb7";   // Symbol 0xB7
const std::string symbol_minus = "\xef\x80\xad"; // Symbol 0x2D
const std::string square = "\xef\x82\xa7";       // 0xA7
const std::string arrow = "\xef\x83\x98";        // Wingdings 0xD8
const std::string check = "\xef\x83\xbc";        // Wingdings 0xFC
const std::string diamond = "\xef\x81\xb5";      // Wingdings 0x75
const std::string clover = "\xef\x81\xb6";       // Wingdings 0x76
const std::string white_square = "\xef\x82\xa8"; // Wingdings 0xA8
const std::string unmapped = "\xef\x80\xa1";     // 0x21
const std::string private_use = "\xee\x80\x80";  // U+E000
} // namespace

TEST(ResolveBullet, a_symbol_font_bullet_becomes_its_unicode_character) {
  EXPECT_EQ(resolve_bullet(symbol_dot, 0, "Symbol"), "•");
  EXPECT_EQ(resolve_bullet(symbol_dot, 1, "'Symbol'"), "•");
  EXPECT_EQ(resolve_bullet(symbol_minus, 0, "symbol"), "−");
  EXPECT_EQ(resolve_bullet(square, 1, "Wingdings"), "▪");
  EXPECT_EQ(resolve_bullet(arrow, 0, "Wingdings"), "➢");
  EXPECT_EQ(resolve_bullet(check, 0, "Wingdings"), "✓");
  EXPECT_EQ(resolve_bullet(diamond, 0, "Wingdings"), "◆");
  EXPECT_EQ(resolve_bullet(clover, 0, "Wingdings"), "❖");
  EXPECT_EQ(resolve_bullet(white_square, 0, "Wingdings"), "◻");
}

TEST(ResolveBullet, the_font_tells_the_same_byte_apart) {
  EXPECT_EQ(resolve_bullet(square, 0, "Symbol"), "♣");
  EXPECT_EQ(resolve_bullet(square, 0, "Wingdings"), "▪");
}

TEST(ResolveBullet, a_symbol_font_is_read_at_its_byte_too) {
  // the bytes as such, which Word writes as often: "v", "§" and "n"
  EXPECT_EQ(resolve_bullet("v", 0, "Wingdings"), "❖");
  EXPECT_EQ(resolve_bullet("\xc2\xa7", 0, "Wingdings"), "▪");
  EXPECT_EQ(resolve_bullet("n", 0, "ZapfDingbats"), "■");
  EXPECT_EQ(resolve_bullet("-", 0, "Symbol"), "−");
}

TEST(ResolveBullet, an_unmapped_symbol_takes_the_level_default) {
  EXPECT_EQ(resolve_bullet(unmapped, 1, "Wingdings"), "◦");
  EXPECT_EQ(resolve_bullet("x", 2, "Wingdings"), "▪");
  // Wingdings 2 and 3 have tables of their own, which are not read
  EXPECT_EQ(resolve_bullet(square, 0, "Wingdings 2"), "•");
}

TEST(ResolveBullet, a_private_use_bullet_in_another_font_takes_the_default) {
  EXPECT_EQ(resolve_bullet(private_use, 0, "OpenSymbol"), "•");
  EXPECT_EQ(resolve_bullet(square, 1, ""), "◦");
  EXPECT_EQ(resolve_bullet(square, 2, "Marlett"), "▪");
}

TEST(ResolveBullet, other_text_is_kept) {
  EXPECT_EQ(resolve_bullet("\xe2\x80\x93", 0, "Symbol"), "\xe2\x80\x93");
  EXPECT_EQ(resolve_bullet("o", 1, "Courier New"), "o");
  EXPECT_EQ(resolve_bullet("v", 1, ""), "v");
  EXPECT_EQ(resolve_bullet("", 1), "◦");
  EXPECT_EQ(resolve_bullet("\xff", 0), "•");
}

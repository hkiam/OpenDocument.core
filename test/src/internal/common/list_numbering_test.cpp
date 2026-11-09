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

TEST(ResolveBullet, a_symbol_font_bullet_becomes_its_unicode_character) {
  // Word's bullets: Symbol's dot and Wingdings' square at private-use points
  EXPECT_EQ(resolve_bullet("", 0, "Symbol"), "•");
  EXPECT_EQ(resolve_bullet("", 1, "'Symbol'"), "•");
  EXPECT_EQ(resolve_bullet("", 1, "Wingdings"), "▪");
  EXPECT_EQ(resolve_bullet("", 0, "Wingdings"), "➢");
  EXPECT_EQ(resolve_bullet("", 0, "Wingdings"), "✔");
  EXPECT_EQ(resolve_bullet("", 0, "Symbol"), "−");
}

TEST(ResolveBullet, the_font_tells_the_same_byte_apart) {
  EXPECT_EQ(resolve_bullet("", 0, "Symbol"), "♣");
  EXPECT_EQ(resolve_bullet("", 0, "Wingdings"), "▪");
  // an unknown font is read as Wingdings, as before the font was asked
  EXPECT_EQ(resolve_bullet("", 0, ""), "▪");
}

TEST(ResolveBullet, an_unmapped_private_use_bullet_takes_the_level_default) {
  EXPECT_EQ(resolve_bullet("", 1, "Wingdings"), "◦");
  EXPECT_EQ(resolve_bullet("", 0, "Symbol"), "•");
  EXPECT_EQ(resolve_bullet("", 2, "Marlett"), "▪");
}

TEST(ResolveBullet, other_text_is_kept) {
  EXPECT_EQ(resolve_bullet("–", 0, "Symbol"), "–");
  EXPECT_EQ(resolve_bullet("o", 1, "Courier New"), "o");
  EXPECT_EQ(resolve_bullet("", 1), "◦");
  EXPECT_EQ(resolve_bullet("\xff", 0), "•");
}

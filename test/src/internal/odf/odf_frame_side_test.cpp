#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// The html of a text document holding one paragraph-anchored frame whose
/// style states @p properties.
std::string html_of_frame(const std::string &name,
                          const std::string &properties) {
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )"
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
      R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0">)"
      R"(<office:automatic-styles><style:style style:name="fr1" )"
      R"(style:family="graphic"><style:graphic-properties )" +
      properties +
      R"(/></style:style></office:automatic-styles><office:body><office:text>)"
      R"(<text:p><draw:frame draw:style-name="fr1" text:anchor-type="char" )"
      R"(svg:width="6cm" svg:height="1.3cm"><draw:text-box><text:p>Logo)"
      R"(</text:p></draw:text-box></draw:frame>Text</text:p></office:text>)"
      R"(</office:body></office:document-content>)";

  const std::string path = (std::filesystem::current_path() / name).string();
  zip::ZipArchive zip;
  zip.insert_file(
      std::end(zip), RelPath("mimetype"),
      std::make_shared<MemoryFile>("application/vnd.oasis.opendocument.text"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  {
    std::ofstream out(path, std::ios::binary);
    zip.save(out);
  }

  std::ostringstream html;
  html::translate(odr::open(path), HtmlConfig())
      .list_views()
      .at(0)
      .write_html(html);
  return html.str();
}

} // namespace

TEST(OdfFrameSide, a_frame_sits_where_its_position_says) {
  const auto html = [](const std::string &name, const std::string &wrap,
                       const std::string &position) {
    return html_of_frame(name, R"(style:wrap=")" + wrap +
                                   R"(" style:horizontal-pos=")" + position +
                                   R"(")");
  };

  // text may flow on the free side: the frame floats to its own
  const std::string dynamic_left =
      html("odf_frame_side_dl.odt", "dynamic", "left");
  EXPECT_NE(dynamic_left.find("float:left"), std::string::npos);
  EXPECT_EQ(dynamic_left.find("float:right"), std::string::npos);
  EXPECT_NE(
      html("odf_frame_side_pr.odt", "parallel", "right").find("float:right"),
      std::string::npos);
  EXPECT_NE(html("odf_frame_side_rl.odt", "right", "left").find("float:left"),
            std::string::npos);

  // text may flow only on the side the frame takes: it stands alone
  const std::string left_left = html("odf_frame_side_ll.odt", "left", "left");
  EXPECT_EQ(left_left.find("float:"), std::string::npos);
  const std::string right_right =
      html("odf_frame_side_rr.odt", "right", "right");
  EXPECT_EQ(right_right.find("float:"), std::string::npos);
  EXPECT_NE(right_right.find("margin-left:auto"), std::string::npos);

  // centred: css cannot flow around it on both sides
  const std::string centre = html("odf_frame_side_dc.odt", "dynamic", "center");
  EXPECT_EQ(centre.find("float:"), std::string::npos);
  EXPECT_NE(centre.find("margin-left:auto;margin-right:auto"),
            std::string::npos);
}

namespace {

/// The side the frame of a flat text document floats to, whose style
/// `child` states @p properties under a parent `base` stating @p base.
std::optional<TextWrap> wrap_of(const std::string &base,
                                const std::string &properties) {
  const Document document =
      odr::open(
          File::from_memory(
              R"(<?xml version="1.0" encoding="UTF-8"?>)"
              R"(<office:document office:mimetype=")"
              R"(application/vnd.oasis.opendocument.text">)"
              R"(<office:styles><style:style style:name="base")"
              R"( style:family="graphic"><style:graphic-properties )" +
              base +
              R"(/></style:style></office:styles><office:automatic-styles>)"
              R"(<style:style style:name="child" style:family="graphic")"
              R"( style:parent-style-name="base"><style:graphic-properties )" +
              properties +
              R"(/></style:style></office:automatic-styles><office:body>)"
              R"(<office:text><text:p><draw:frame draw:style-name="child")"
              R"( text:anchor-type="char" svg:width="6cm" svg:height="1cm">)"
              R"(<draw:text-box><text:p>Logo</text:p></draw:text-box>)"
              R"(</draw:frame>Text</text:p></office:text></office:body>)"
              R"(</office:document>)"))
          .as_document_file()
          .document();
  return document.root_element()
      .first_child()
      .first_child()
      .as_frame()
      .style()
      .text_wrap;
}

} // namespace

TEST(OdfFrameSide, a_wrap_its_parent_style_states_counts_too) {
  // the header logo of a LibreOffice template: wrap on the parent style
  EXPECT_EQ(
      wrap_of(R"(style:wrap="dynamic")", R"(style:horizontal-pos="left")"),
      TextWrap::after);
  EXPECT_EQ(wrap_of(R"(style:wrap="left")", R"(style:horizontal-pos="left")"),
            TextWrap::none);
}

TEST(OdfFrameSide, a_frame_placed_by_its_offset_keeps_its_wrap) {
  // `from-left` states no side, so the wrap decides as before
  EXPECT_EQ(
      wrap_of(R"(style:wrap="right")", R"(style:horizontal-pos="from-left")"),
      wrap_of(R"(style:wrap="right")", ""));
}

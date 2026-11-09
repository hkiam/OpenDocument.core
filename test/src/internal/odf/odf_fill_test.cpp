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
#include <sstream>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal;

namespace {

constexpr const char *namespaces =
    R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
    R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
    R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
    R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
    R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0")";

/// A drawing whose `styles.xml` holds @p styles under `office:styles` and
/// whose page holds @p body; with @p generator, a `meta.xml` naming it.
std::string write_odg(const std::string &name, const std::string &styles,
                      const std::string &body,
                      const std::string &generator = {}) {
  const std::string content =
      std::string(R"(<?xml version="1.0" encoding="UTF-8"?>)") +
      "<office:document-content " + namespaces + ">" +
      R"(<office:body><office:drawing><draw:page>)" + body +
      R"(</draw:page></office:drawing></office:body>)"
      R"(</office:document-content>)";
  const std::string styles_xml =
      std::string(R"(<?xml version="1.0" encoding="UTF-8"?>)") +
      "<office:document-styles " + namespaces + "><office:styles>" + styles +
      "</office:styles></office:document-styles>";

  const std::string path = (std::filesystem::current_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(std::end(zip), RelPath("mimetype"),
                  std::make_shared<MemoryFile>(
                      "application/vnd.oasis.opendocument.graphics"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  zip.insert_file(std::end(zip), RelPath("styles.xml"),
                  std::make_shared<MemoryFile>(styles_xml));
  if (!generator.empty()) {
    zip.insert_file(
        std::end(zip), RelPath("meta.xml"),
        std::make_shared<MemoryFile>(
            R"(<?xml version="1.0" encoding="UTF-8"?><office:document-meta )"
            R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
            R"(xmlns:meta="urn:oasis:names:tc:opendocument:xmlns:meta:1.0">)"
            R"(<office:meta><meta:generator>)" +
            generator +
            R"(</meta:generator></office:meta></office:document-meta>)"));
  }
  std::ofstream out(path, std::ios::binary);
  zip.save(out);

  return path;
}

std::vector<GraphicStyle> styles_of(const Document &document) {
  std::vector<GraphicStyle> result;
  for (const Element child : document.root_element().first_child().children()) {
    result.push_back(child.as_frame().style());
  }
  return result;
}

std::string rect(const std::string &style) {
  return R"(<draw:rect draw:style-name=")" + style +
         R"(" svg:x="1cm" svg:y="1cm" svg:width="4cm" svg:height="2cm"/>)";
}

/// The html of the first view of the file at @p path.
std::string render(const std::string &path) {
  std::ostringstream html;
  html::translate(odr::open(path), HtmlConfig())
      .list_views()
      .at(0)
      .write_html(html);
  return html.str();
}

} // namespace

TEST(OdfFill, a_gradient_is_read_with_its_intensities_and_its_angle) {
  const std::string path = write_odg(
      "odf_fill_gradient.odg",
      R"(<draw:gradient draw:name="G" draw:style="axial" )"
      R"(draw:start-color="#ff0000" draw:end-color="#0000ff" )"
      R"(draw:start-intensity="50%" draw:end-intensity="100%" )"
      R"(draw:angle="300" draw:border="20%" draw:cx="10%" draw:cy="90%"/>)"
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="gradient" )"
      R"(draw:fill-gradient-name="G"/></style:style>)",
      rect("g"), "LibreOffice/6.4.7.2$Linux_X86_64");

  const Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  ASSERT_TRUE(style.fill_gradient.has_value());
  EXPECT_FALSE(style.fill_hatch.has_value());
  const GradientFill &gradient = *style.fill_gradient;
  EXPECT_EQ(gradient.style, GradientStyle::axial);
  EXPECT_EQ(gradient.start_color.rgb(), 0x800000u);
  EXPECT_EQ(gradient.end_color.rgb(), 0x0000ffu);
  EXPECT_DOUBLE_EQ(gradient.angle, 30);
  EXPECT_DOUBLE_EQ(gradient.border, 0.2);
  EXPECT_DOUBLE_EQ(gradient.center_x, 0.1);
  EXPECT_DOUBLE_EQ(gradient.center_y, 0.9);
}

TEST(OdfFill, a_gradient_angle_with_a_unit_is_read_in_that_unit) {
  const std::string path =
      write_odg("odf_fill_gradient_angle.odg",
                R"(<draw:gradient draw:name="D" draw:angle="27deg"/>)"
                R"(<draw:gradient draw:name="G" draw:angle="100grad"/>)"
                R"(<style:style style:name="d" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="D"/></style:style>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G"/></style:style>)",
                rect("d") + rect("g"));

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  EXPECT_DOUBLE_EQ(styles.at(0).fill_gradient.value().angle, 27);
  EXPECT_DOUBLE_EQ(styles.at(1).fill_gradient.value().angle, 90);
}

TEST(OdfFill, the_fill_kind_and_the_gradient_name_cascade_independently) {
  const std::string path = write_odg(
      "odf_fill_cascade.odg",
      R"(<draw:gradient draw:name="G" draw:style="linear"/>)"
      R"(<style:style style:name="base" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill-gradient-name="G"/>)"
      R"(</style:style>)"
      R"(<style:style style:name="gradient" style:family="graphic" )"
      R"(style:parent-style-name="base">)"
      R"(<style:graphic-properties draw:fill="gradient"/></style:style>)"
      R"(<style:style style:name="solid" style:family="graphic" )"
      R"(style:parent-style-name="gradient">)"
      R"(<style:graphic-properties draw:fill="solid" )"
      R"(draw:fill-color="#00ff00"/></style:style>)",
      rect("gradient") + rect("solid"));

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  EXPECT_TRUE(styles.at(0).fill_gradient.has_value());
  EXPECT_FALSE(styles.at(1).fill_gradient.has_value());
  ASSERT_TRUE(styles.at(1).fill_color.has_value());
  EXPECT_EQ(styles.at(1).fill_color->alpha, 255);
}

TEST(OdfFill, a_solid_hatch_paints_its_fill_color_behind_the_lines) {
  const std::string path = write_odg(
      "odf_fill_hatch.odg",
      R"(<draw:hatch draw:name="H" draw:style="double" draw:color="#3465a4" )"
      R"(draw:distance="0.15cm" draw:rotation="450"/>)"
      R"(<style:style style:name="solid" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" draw:fill-hatch-name="H" )"
      R"(draw:fill-hatch-solid="true" draw:fill-color="#009eda"/>)"
      R"(</style:style>)"
      R"(<style:style style:name="lines" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="hatch" draw:fill-hatch-name="H" )"
      R"(draw:fill-color="#009eda"/></style:style>)",
      rect("solid") + rect("lines"));

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).fill_hatch.has_value());
  const HatchFill &hatch = *styles.at(0).fill_hatch;
  EXPECT_EQ(hatch.style, HatchStyle::crossed);
  EXPECT_EQ(hatch.color.rgb(), 0x3465a4u);
  EXPECT_EQ(hatch.distance.to_string(), "0.15cm");
  EXPECT_DOUBLE_EQ(hatch.rotation, 45);
  ASSERT_TRUE(styles.at(0).fill_color.has_value());
  EXPECT_EQ(styles.at(0).fill_color->alpha, 255);

  ASSERT_TRUE(styles.at(1).fill_hatch.has_value());
  ASSERT_TRUE(styles.at(1).fill_color.has_value());
  EXPECT_EQ(styles.at(1).fill_color->alpha, 0);
}

TEST(OdfFill, a_gradient_that_names_nothing_paints_nothing) {
  const std::string path = write_odg(
      "odf_fill_missing.odg",
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="gradient" )"
      R"(draw:fill-gradient-name="missing" draw:fill-color="#00ff00"/>)"
      R"(</style:style>)",
      rect("g"));

  const Document document = odr::open(path).as_document_file().document();
  const GraphicStyle style = styles_of(document).at(0);

  EXPECT_FALSE(style.fill_gradient.has_value());
  ASSERT_TRUE(style.fill_color.has_value());
  EXPECT_EQ(style.fill_color->alpha, 0);
}

TEST(OdfFill, a_visible_shadow_is_read_with_its_offsets_and_opacity) {
  const std::string path = write_odg(
      "odf_fill_shadow.odg",
      R"(<style:style style:name="base" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow-offset-x="0cm" )"
      R"(draw:shadow-offset-y="-0.07cm" draw:shadow-color="#f49100" )"
      R"(draw:shadow-opacity="50%"/></style:style>)"
      R"(<style:style style:name="shadow" style:family="graphic" )"
      R"(style:parent-style-name="base">)"
      R"(<style:graphic-properties draw:shadow="visible"/></style:style>)"
      R"(<style:style style:name="hidden" style:family="graphic" )"
      R"(style:parent-style-name="shadow">)"
      R"(<style:graphic-properties draw:shadow="hidden"/></style:style>)",
      rect("shadow") + rect("hidden"));

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).shadow.has_value());
  const Shadow &shadow = *styles.at(0).shadow;
  EXPECT_EQ(shadow.offset_x.to_string(), "0cm");
  EXPECT_EQ(shadow.offset_y.to_string(), "-0.07cm");
  EXPECT_EQ(shadow.color.rgb(), 0xf49100u);
  EXPECT_EQ(shadow.color.alpha, 128);
  EXPECT_FALSE(styles.at(1).shadow.has_value());
}

TEST(OdfFill, a_shape_stating_no_fill_is_filled_and_a_frame_is_not) {
  // As LibreOffice writes a Writer header's shape: a fill color, no
  // `draw:fill`, the solid default left unwritten.
  const std::string path = write_odg(
      "odf_fill_default.odg",
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill-color="#fffc00"/>)"
      R"(</style:style>)",
      rect("g") + R"(<draw:frame draw:style-name="g" svg:x="1cm" svg:y="1cm" )"
                  R"(svg:width="4cm" svg:height="2cm"/>)");

  const Document document = odr::open(path).as_document_file().document();
  const std::vector<GraphicStyle> styles = styles_of(document);

  ASSERT_TRUE(styles.at(0).fill_color.has_value());
  EXPECT_EQ(styles.at(0).fill_color->alpha, 255);
  ASSERT_TRUE(styles.at(1).fill_color.has_value());
  EXPECT_EQ(styles.at(1).fill_color->alpha, 0);
}

TEST(OdfFill,
     a_unitless_gradient_angle_is_in_degrees_unless_its_producer_wrote_tenths) {
  const std::string styles =
      R"(<draw:gradient draw:name="G" draw:angle="300"/>)"
      R"(<style:style style:name="g" style:family="graphic">)"
      R"(<style:graphic-properties draw:fill="gradient" )"
      R"(draw:fill-gradient-name="G"/></style:style>)";
  const auto angle = [&](const std::string &name,
                         const std::string &generator) {
    const Document document =
        odr::open(write_odg(name, styles, rect("g"), generator))
            .as_document_file()
            .document();
    return styles_of(document).at(0).fill_gradient.value().angle;
  };

  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_unknown.odg", ""), 300);
  EXPECT_DOUBLE_EQ(
      angle("odf_fill_angle_lo7.odg", "LibreOffice/7.3.7.2$Linux_X86_64"), 300);
  EXPECT_DOUBLE_EQ(angle("odf_fill_angle_lo6.odg", "LibreOffice/6.4.7.2"), 30);
  EXPECT_DOUBLE_EQ(
      angle("odf_fill_angle_aoo.odg", "Apache_OpenOffice/4.1.15$Win32"), 30);
}

TEST(OdfFill, a_shadow_stated_on_a_frame_is_not_cast_again_by_its_content) {
  const std::string path = write_odg(
      "odf_fill_shadow_frame.odg",
      R"(<style:style style:name="s" style:family="graphic">)"
      R"(<style:graphic-properties draw:shadow="visible"/></style:style>)",
      R"(<draw:frame draw:style-name="s" svg:x="1cm" svg:y="1cm" )"
      R"(svg:width="4cm" svg:height="2cm"><draw:text-box>)"
      R"(<text:p>A</text:p></draw:text-box></draw:frame>)");

  const Document document = odr::open(path).as_document_file().document();
  const Frame frame =
      document.root_element().first_child().first_child().as_frame();

  ASSERT_TRUE(frame.style().shadow.has_value());
  EXPECT_EQ(frame.style().shadow->offset_x.to_string(), "0cm");
  EXPECT_EQ(frame.style().shadow->color.rgb(), 0x000000u);
}

TEST(OdfFill, a_shape_paints_its_gradient_on_a_layer_masked_to_it) {
  const std::string path =
      write_odg("odf_fill_html.odg",
                R"(<draw:gradient draw:name="G" draw:style="linear" )"
                R"(draw:start-color="#ff0000" draw:end-color="#0000ff"/>)"
                R"(<style:style style:name="g" style:family="graphic">)"
                R"(<style:graphic-properties draw:fill="gradient" )"
                R"(draw:fill-gradient-name="G" draw:shadow="visible" )"
                R"(draw:shadow-offset-x="0.1cm"/></style:style>)",
                R"(<draw:ellipse draw:style-name="g" svg:x="1cm" svg:y="1cm" )"
                R"(svg:width="4cm" svg:height="2cm"/>)"
                R"(<draw:path draw:style-name="g" svg:x="1cm" svg:y="4cm" )"
                R"(svg:width="4cm" svg:height="2cm" svg:viewBox="0 0 400 200" )"
                R"(svg:d="M0 0 L400 200"/>)");

  const std::string page = render(path);

  // one layer, for the closed ellipse; the open path is not filled
  const std::size_t layer = page.find(R"(class="odr-fill")");
  ASSERT_NE(layer, std::string::npos);
  EXPECT_EQ(page.find(R"(class="odr-fill")", layer + 1), std::string::npos);
  EXPECT_NE(page.find("linear-gradient(180deg,#ff0000 0%,#0000ff 100%)"),
            std::string::npos);
  EXPECT_NE(page.find("mask-image:url('data:image/svg+xml,"),
            std::string::npos);
  // the shape's svg only strokes, and the shadow stays off its box
  EXPECT_NE(page.find("fill:none;"), std::string::npos);
  EXPECT_NE(page.find("filter:drop-shadow(0.1cm 0cm 0cm #000000)"),
            std::string::npos);
  EXPECT_EQ(page.find("box-shadow", page.find("<body")), std::string::npos);
}

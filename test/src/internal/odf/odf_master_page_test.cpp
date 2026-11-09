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

using namespace odr;
using namespace odr::internal;

namespace {

/// A text document with a first page and a standard page, each with a
/// header, the standard one with a footer as well.
std::string write_odt(const std::string &name) {
  const std::string namespaces =
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" )"
      R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0")";
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )" +
      namespaces +
      R"(><office:automatic-styles><style:style style:name="P1" )"
      R"(style:family="paragraph" style:master-page-name="First"/>)"
      R"(</office:automatic-styles><office:body><office:text>)"
      R"(<text:p text:style-name="P1">Body</text:p></office:text>)"
      R"(</office:body></office:document-content>)";
  const std::string styles =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-styles )" +
      namespaces +
      R"(><office:automatic-styles><style:page-layout style:name="pm1">)"
      R"(<style:page-layout-properties fo:page-width="21cm" )"
      R"(fo:page-height="29.7cm"/><style:header-style>)"
      R"(<style:header-footer-properties fo:min-height="0.6cm" )"
      R"(fo:margin-bottom="0.5cm"/></style:header-style><style:footer-style>)"
      R"(<style:header-footer-properties svg:height="1cm" )"
      R"(fo:margin-top="0.3cm"/></style:footer-style></style:page-layout>)"
      R"(</office:automatic-styles><office:master-styles>)"
      R"(<style:master-page style:name="Standard" style:page-layout-name="pm1">)"
      R"(<style:header><text:p>Standard header</text:p></style:header>)"
      R"(<style:footer><text:p>Standard footer</text:p></style:footer>)"
      R"(</style:master-page>)"
      R"(<style:master-page style:name="First" style:page-layout-name="pm1" )"
      R"(style:next-style-name="Standard">)"
      R"(<style:header><text:p>First header</text:p></style:header>)"
      R"(</style:master-page></office:master-styles></office:document-styles>)";

  const std::string path = (std::filesystem::current_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(
      std::end(zip), RelPath("mimetype"),
      std::make_shared<MemoryFile>("application/vnd.oasis.opendocument.text"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  zip.insert_file(std::end(zip), RelPath("styles.xml"),
                  std::make_shared<MemoryFile>(styles));
  std::ofstream out(path, std::ios::binary);
  zip.save(out);

  return path;
}

std::string text_of(const Element &element) {
  std::string result;
  for (const Element child : element.children()) {
    if (child.type() == ElementType::text) {
      result += child.as_text().content();
    }
    result += text_of(child);
  }
  return result;
}

} // namespace

TEST(OdfMasterPage, the_header_and_footer_are_their_own_elements) {
  const Document document = odr::open(write_odt("odf_master_page_header.odt"))
                                .as_document_file()
                                .document();
  const MasterPage first =
      document.root_element().as_text_root().first_master_page();

  ASSERT_TRUE(first.header());
  EXPECT_EQ(text_of(first.header()), "First header");
  EXPECT_FALSE(first.footer());

  const MasterPage standard = first.next_master_page();
  EXPECT_EQ(text_of(standard.header()), "Standard header");
  EXPECT_EQ(text_of(standard.footer()), "Standard footer");
  // naming no next page keeps this one
  EXPECT_EQ(text_of(standard.next_master_page().header()), "Standard header");
}

TEST(OdfMasterPage, the_layout_carries_the_header_and_footer_boxes) {
  const Document document = odr::open(write_odt("odf_master_page_layout.odt"))
                                .as_document_file()
                                .document();
  const PageLayout layout =
      document.root_element().as_text_root().page_layout();

  ASSERT_TRUE(layout.header_min_height.has_value());
  ASSERT_TRUE(layout.header_spacing.has_value());
  ASSERT_TRUE(layout.footer_min_height.has_value());
  ASSERT_TRUE(layout.footer_spacing.has_value());
  EXPECT_EQ(layout.header_min_height->to_string(), "0.6cm");
  EXPECT_EQ(layout.header_spacing->to_string(), "0.5cm");
  EXPECT_EQ(layout.footer_min_height->to_string(), "1cm");
  EXPECT_EQ(layout.footer_spacing->to_string(), "0.3cm");
}

namespace {

/// A text document of three pages: a title page whose master has a first
/// page header, a page break onto the standard page, then a paragraph naming
/// the landscape master.
std::string write_pages_odt(const std::string &name) {
  const std::string namespaces =
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" )"
      R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0")";
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )" +
      namespaces +
      R"(><office:automatic-styles>)"
      R"(<style:style style:name="Break" style:family="paragraph">)"
      R"(<style:paragraph-properties fo:break-before="page"/></style:style>)"
      R"(<style:style style:name="Wide" style:family="paragraph" )"
      R"(style:master-page-name="Landscape"/>)"
      R"(</office:automatic-styles><office:body><office:text>)"
      R"(<text:p>Title</text:p>)"
      R"(<text:p text:style-name="Break">Second</text:p>)"
      R"(<text:p text:style-name="Wide">Third</text:p>)"
      R"(</office:text></office:body></office:document-content>)";
  const std::string styles =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-styles )" +
      namespaces +
      R"(><office:automatic-styles>)"
      R"(<style:page-layout style:name="pm1"><style:page-layout-properties )"
      R"(fo:page-width="21cm" fo:page-height="29.7cm"/><style:header-style>)"
      R"(<style:header-footer-properties fo:min-height="2cm" )"
      R"(fo:margin-bottom="1cm"/></style:header-style><style:footer-style>)"
      R"(<style:header-footer-properties fo:margin-top="0.5cm"/>)"
      R"(</style:footer-style></style:page-layout>)"
      R"(<style:page-layout style:name="pm2"><style:page-layout-properties )"
      R"(fo:page-width="29.7cm" fo:page-height="21cm"/></style:page-layout>)"
      R"(</office:automatic-styles><office:master-styles>)"
      R"(<style:master-page style:name="Standard" style:page-layout-name="pm1">)"
      R"(<style:header><text:p>Running head</text:p></style:header>)"
      R"(<style:header-first><text:p>Title head</text:p></style:header-first>)"
      R"(<style:footer><text:p>Foot</text:p></style:footer>)"
      R"(</style:master-page>)"
      R"(<style:master-page style:name="Landscape" )"
      R"(style:page-layout-name="pm2"/>)"
      R"(</office:master-styles></office:document-styles>)";

  const std::string path = (std::filesystem::current_path() / name).string();
  zip::ZipArchive zip;
  zip.insert_file(
      std::end(zip), RelPath("mimetype"),
      std::make_shared<MemoryFile>("application/vnd.oasis.opendocument.text"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  zip.insert_file(std::end(zip), RelPath("styles.xml"),
                  std::make_shared<MemoryFile>(styles));
  std::ofstream out(path, std::ios::binary);
  zip.save(out);
  return path;
}

std::string html_of_pages(const std::string &path, const bool editable) {
  HtmlConfig config;
  config.text_document_margin = true;
  config.editable = editable;
  std::ostringstream html;
  html::translate(odr::open(path), config).list_views().at(0).write_html(html);
  return html.str();
}

std::size_t count(const std::string &text, const std::string &part) {
  std::size_t result = 0;
  for (std::size_t at = text.find(part); at != std::string::npos;
       at = text.find(part, at + 1)) {
    ++result;
  }
  return result;
}

} // namespace

TEST(OdfMasterPage, each_page_box_carries_its_master_pages_header_and_footer) {
  const std::string page =
      html_of_pages(write_pages_odt("odf_master_page_boxes.odt"), false);

  // the title page takes the first page header, the next the running one,
  // the landscape page has none
  EXPECT_EQ(count(page, "Title head"), 1);
  EXPECT_EQ(count(page, "Running head"), 1);
  EXPECT_EQ(count(page, "Foot"), 2);
  EXPECT_LT(page.find("Title head"), page.find("Title<"));
  EXPECT_LT(page.find("Running head"), page.find("Second"));
  // a paragraph naming a master page opens a page of its size
  EXPECT_EQ(count(page, R"(class="odr-page-outer")"), 3);
  EXPECT_NE(page.find("width:29.7cm"), std::string::npos);
  // the height holds the spacing, as in LibreOffice
  EXPECT_NE(
      page.find("box-sizing:border-box;min-height:2cm;padding-bottom:1cm"),
      std::string::npos);
  EXPECT_NE(page.find("padding-top:0.5cm"), std::string::npos);
}

TEST(OdfMasterPage, a_header_carries_no_editing_address) {
  const std::string page =
      html_of_pages(write_pages_odt("odf_master_page_editable.odt"), true);

  const std::size_t header = page.find(R"(class="odr-page-header")");
  ASSERT_NE(header, std::string::npos);
  const std::size_t header_end = page.find("odr-page-body", header);
  const std::string inside = page.substr(header, header_end - header);
  EXPECT_NE(inside.find(R"(contenteditable="false")"), std::string::npos);
  EXPECT_EQ(inside.find("data-odr-id"), std::string::npos);
}

TEST(OdfMasterPage, a_document_without_a_header_writes_no_header_box) {
  const std::string path =
      (std::filesystem::current_path() / "odf_master_page_none.odt").string();
  zip::ZipArchive zip;
  zip.insert_file(
      std::end(zip), RelPath("mimetype"),
      std::make_shared<MemoryFile>("application/vnd.oasis.opendocument.text"));
  zip.insert_file(
      std::end(zip), RelPath("content.xml"),
      std::make_shared<MemoryFile>(
          R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )"
          R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
          R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0">)"
          R"(<office:body><office:text><text:p>Body</text:p></office:text>)"
          R"(</office:body></office:document-content>)"));
  {
    std::ofstream out(path, std::ios::binary);
    zip.save(out);
  }

  const std::string page = html_of_pages(path, false);
  EXPECT_NE(page.find(R"(class="odr-page-inner")"), std::string::npos);
  EXPECT_EQ(page.find("odr-page-body"), std::string::npos);
}

namespace {

/// A flat text document with @p body, the automatic styles @p styles and the
/// master pages @p master_pages.
std::string flat_pages(const std::string &body, const std::string &styles,
                       const std::string &master_pages) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.text">)"
         R"(<office:automatic-styles>)" +
         styles + R"(</office:automatic-styles><office:master-styles>)" +
         master_pages + R"(</office:master-styles><office:body><office:text>)" +
         body + R"(</office:text></office:body></office:document>)";
}

Document document_of(const std::string &source) {
  return odr::open(File::from_memory(source)).as_document_file().document();
}

std::string html_of(const std::string &source) {
  HtmlConfig config;
  config.text_document_margin = true;
  std::ostringstream html;
  html::translate(odr::open(File::from_memory(source)), config)
      .list_views()
      .at(0)
      .write_html(html);
  return html.str();
}

/// A paragraph style naming the master page @p master_page.
std::string opening(const std::string &style, const std::string &master_page) {
  return R"(<style:style style:name=")" + style +
         R"(" style:family="paragraph" style:master-page-name=")" +
         master_page + R"("/>)";
}

} // namespace

TEST(OdfMasterPage, an_empty_paragraph_keeps_the_default_on_page_one) {
  // LibreOffice: the first content's master page is page one's, and an empty
  // paragraph is content
  const std::string html = html_of(flat_pages(
      R"(<text:p/><text:p text:style-name="Wide">Landscape</text:p>)",
      opening("Wide", "Landscape"),
      R"(<style:master-page style:name="Standard"><style:header>)"
      R"(<text:p>Standard head</text:p></style:header></style:master-page>)"
      R"(<style:master-page style:name="Landscape"><style:header>)"
      R"(<text:p>Wide head</text:p></style:header></style:master-page>)"));
  EXPECT_EQ(count(html, "Standard head"), 1U);
  EXPECT_EQ(count(html, "Wide head"), 1U);
}

TEST(OdfMasterPage, standard_is_the_default_master_page) {
  const std::string html = html_of(flat_pages(
      R"(<text:p>Body</text:p>)", "",
      R"(<style:master-page style:name="First"><style:header>)"
      R"(<text:p>First head</text:p></style:header></style:master-page>)"
      R"(<style:master-page style:name="Standard"><style:header>)"
      R"(<text:p>Standard head</text:p></style:header></style:master-page>)"));
  EXPECT_EQ(count(html, "Standard head"), 1U);
  EXPECT_EQ(count(html, "First head"), 0U);
}

TEST(OdfMasterPage, an_empty_master_page_name_opens_no_page) {
  // the style states that it opens none, so its parent's name does not count
  const Document document = document_of(flat_pages(
      R"(<text:p>one</text:p><text:p text:style-name="Child">two</text:p>)",
      opening("Parent", "Landscape") +
          R"(<style:style style:name="Child" style:family="paragraph")"
          R"( style:parent-style-name="Parent" style:master-page-name=""/>)",
      R"(<style:master-page style:name="Standard"/>)"
      R"(<style:master-page style:name="Landscape"/>)"));
  EXPECT_FALSE(document.root_element()
                   .first_child()
                   .next_sibling()
                   .as_paragraph()
                   .master_page());
}

TEST(OdfMasterPage, naming_the_master_page_in_use_again_opens_no_run) {
  // LibreOffice shows the first page's header on page one only
  const std::string html = html_of(flat_pages(
      R"(<text:p>one</text:p><text:p text:style-name="Again">two</text:p>)",
      opening("Again", "Standard"),
      R"(<style:master-page style:name="Standard"><style:header>)"
      R"(<text:p>Running head</text:p></style:header><style:header-first>)"
      R"(<text:p>Title head</text:p></style:header-first>)"
      R"(</style:master-page>)"));
  EXPECT_EQ(count(html, "Title head"), 1U);
  EXPECT_EQ(count(html, "Running head"), 1U);
}

TEST(OdfMasterPage, a_first_page_header_needs_a_header) {
  const std::string html = html_of(flat_pages(
      R"(<text:p>one</text:p>)", "",
      R"(<style:master-page style:name="Standard"><loext:header-first>)"
      R"(<text:p>Title head</text:p></loext:header-first>)"
      R"(</style:master-page>)"));
  EXPECT_EQ(count(html, "Title head"), 0U);
}

TEST(OdfMasterPage, loext_names_a_first_page_header_too) {
  const std::string html = html_of(flat_pages(
      R"(<text:p>one</text:p>)", "",
      R"(<style:master-page style:name="Standard"><style:header>)"
      R"(<text:p>Running head</text:p></style:header><loext:header-first>)"
      R"(<text:p>Title head</text:p></loext:header-first>)"
      R"(</style:master-page>)"));
  EXPECT_EQ(count(html, "Title head"), 1U);
  EXPECT_EQ(count(html, "Running head"), 0U);
}

TEST(OdfMasterPage, master_pages_alternate_and_an_unknown_next_is_the_same) {
  const Document document = document_of(flat_pages(
      R"(<text:p>one</text:p>)", "",
      R"(<style:master-page style:name="Standard")"
      R"( style:next-style-name="Left"/><style:master-page style:name="Left")"
      R"( style:next-style-name="Standard"/><style:master-page)"
      R"( style:name="Lost" style:next-style-name="Nowhere"/>)"));
  const MasterPage standard =
      document.root_element().as_text_root().first_master_page();
  EXPECT_EQ(standard.next_master_page().next_master_page(), standard);
  EXPECT_NE(standard.next_master_page(), standard);
}

TEST(OdfMasterPage, a_later_page_without_a_size_keeps_its_margins) {
  const std::string html = html_of(flat_pages(
      R"(<text:p>one</text:p><text:p text:style-name="Narrow">two</text:p>)",
      opening("Narrow", "Narrow") +
          R"(<style:page-layout style:name="pm1"><style:page-layout-properties)"
          R"( fo:page-width="21cm" fo:page-height="29.7cm")"
          R"( fo:margin-left="4.5cm"/></style:page-layout>)"
          R"(<style:page-layout style:name="pm2"><style:page-layout-properties)"
          R"( fo:margin-left="2cm"/></style:page-layout>)",
      R"(<style:master-page style:name="Standard")"
      R"( style:page-layout-name="pm1"/><style:master-page style:name="Narrow")"
      R"( style:page-layout-name="pm2"/>)"));
  EXPECT_EQ(count(html, R"(class="odr-page-inner" style="margin-left:4.5cm;")"),
            1U);
  EXPECT_EQ(count(html, R"(class="odr-page-inner" style="margin-left:2cm;")"),
            1U);
  EXPECT_EQ(count(html, R"(style="width:21cm;min-height:29.7cm;")"), 2U);
}

TEST(OdfMasterPage, a_footer_numbers_its_lists) {
  const Document document = document_of(flat_pages(
      R"(<text:p>one</text:p>)",
      R"(<text:list-style style:name="L1"><text:list-level-style-number)"
      R"( text:level="1" style:num-format="1" style:num-suffix="."/>)"
      R"(</text:list-style>)",
      R"(<style:master-page style:name="Standard"><style:footer>)"
      R"(<text:list text:style-name="L1"><text:list-item><text:p>a</text:p>)"
      R"(</text:list-item><text:list-item><text:p>b</text:p>)"
      R"(</text:list-item></text:list></style:footer></style:master-page>)"));
  const Element list = document.root_element()
                           .as_text_root()
                           .first_master_page()
                           .footer()
                           .first_child();
  ASSERT_EQ(list.type(), ElementType::list);
  EXPECT_EQ(list.first_child().as_list_item().marker(), "1.");
  EXPECT_EQ(list.first_child().next_sibling().as_list_item().marker(), "2.");
}

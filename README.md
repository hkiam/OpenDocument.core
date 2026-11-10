# Before and after images for the pull requests

Images for the pull request descriptions, one folder per pull request. Each
image shows three columns: **before**, **after** and **LibreOffice 26.2.3.2**
(build 70e089b17412e4cb7773e41413306b17a2328c34). This branch holds nothing
else and is not meant to be merged.

## How they were made

- **before**: `translate` built from `main` at `ad1d8edd`. For the pull
  requests that build on another one (`fix/odf-field-text` on
  `feat/odf-header-footer`, `fix/symbol-font-bullets` on `feat/odf-numbering`)
  before is that base branch; the column says so.
- **after**: `translate` built from the pull request's branch.
- Where the page layout matters (marked *pages* below), `translate` ran with
  `text_document_margin` on and the viewport fitted to the view, a local
  two-line change to `cli/src/translate.cpp` that is not part of any pull
  request.
- The HTML was rendered with WebKit on macOS; LibreOffice converted each file
  to PDF headless with a fresh profile (en-US for the spreadsheet).

## Sources

Only public files and files written for these images:

- **LibreOffice's test data** (MPL-2.0), core at
  `196fab748dfd639ce639bf37c1caa93ea2fbc46c`.
- **The project's public corpus**, `test/data/input/odr-public/`.
- **Minimal documents** written for these images; they are in the folders
  next to their images.

| Image | Source |
| --- | --- |
| `01-utf8/tdf129357-opens.png` | LibreOffice `sw/qa/extras/layout/data/tdf129357.fodt`; before is the output of `translate`, which refused the file |
| `02-fills/shapes-test-gradients.png` | LibreOffice `sd/qa/unit/data/odp/shapes-test.odp`, slide 5 |
| `02-fills/shapes-test-hatches.png` | the same, slide 6 |
| `02-fills/shadow-and-default-fill.png` | `02-fills/fills-shadow-default.fodg` |
| `03-stroke/rhbz1870501-connectors.png` | LibreOffice `sd/qa/unit/data/odg/rhbz1870501.odg` |
| `03-stroke/style-various-1-slide3-hairline.png` | corpus `odp/style-various-1.odp`, slide 3, magnified |
| `04-paper/margins-only-takes-a4.png` | `04-paper/paper-margins-only.fodt` (*pages*) |
| `04-paper/drawing-without-page-layout.png` | `04-paper/paper-no-layout.fodg` |
| `05-header/style-various-1-header.png` | corpus `odt/style-various-1.odt` (*pages*) |
| `05-header/style-various-1-footer.png` | the same |
| `05-header/mixed-layout-landscape-box.png` | corpus `odt/mixed-layout.odt` (*pages*) |
| `06-fields/fields-show-their-text.png` | `06-fields/fields.fodt` (*pages*) |
| `07-side/frame-side-left-centre-right.png` | `07-side/frame-side.fodt` (*pages*) |
| `08-placeholders/master-prompts-under-slide.png` | `08-placeholders/master-placeholders.fodp` |
| `09-numbering/outline-and-word-list-numbering.png` | `09-numbering/numbering.fodt` (*pages*) |
| `10-bullets/file-sample-100kB-bullets.png` | corpus `odt/file-sample_100kB.odt` (*pages*) |
| `10-bullets/symbol-and-wingdings-bullets.png` | `10-bullets/symbol-bullets.fodt` (*pages*) |
| `11-uncached/cells-without-cached-text.png` | `11-uncached/uncached-cells.fods` |
| `12-font-family/image-1-title-font.png` | corpus `odp/image-1.odp`, slide 1 |

## Differences from LibreOffice the images show

They are named as limits in the pull requests:

- 06: a page number in a footer stays empty ("Page  of 1"); a page box is a
  run between breaks, not a page.
- 07: LibreOffice lets text flow beside a centred frame; the HTML stands it
  alone, as css cannot flow around it.
- 09: the heading's number is followed by a tab; `text:label-followed-by` is
  not read.
- 02: 3D extrusions and bevels are not drawn; 10: the substitute fonts draw
  some bullets smaller.

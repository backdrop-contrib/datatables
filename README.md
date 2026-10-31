# DataTables

This module provides Views integration for the DataTables library, which provides advanced interaction controls to HTML tables such as dynamic pagination, on-the-fly filtering, and column sorting.

Version 2.1 of this module introduces some more useful features:

- **[Server-side processing](#server-side-processing)** which draws each table page using an Ajax request to the server and therefore is useful to show large data sets.
- **[Scroller](#scroller)** which draws the table as a scrolling container. This feature can be enabled independently of server-side-processing but they naturally fit well together, as [DataTables example with both features](https://datatables.net/extensions/scroller/examples/initialisation/server-side_processing.html) demonstrates.
- **[Column filters](#column-filters)** add a quick filter in a second header row, with some basic comparison operators.
- **[Responsive behaviour](#responsive-behaviour)** when the table is too wide for its container (also while the window is being resized).
- [**Switching between core Tables and DataTables display**](#switching-between-table-and-datatable) preserves the settings, so trying it out is easy.

For full documentation and examples, visit the DataTables website: [https://datatables.net/](https://datatables.net/).

## Installation

Install this module using the [official Backdrop CMS instructions](https://docs.backdropcms.org/documentation/extend-with-modules).

There are two ways you can use DataTables:

1. Create a View and use the DataTables display format.
2. Use theme_datatable function in your own module code. Detailed documentation is included with the function.

## Features

### Server-side processing

Whether DataTables works in the browser or asks the server for every page is derived from the Views settings:

- **Display uses AJAX** (Advanced settings: *Use AJAX*) and the pagination style is not *No Pagination*: **server-side processing**.
- Otherwise: all rows are loaded at once and DataTables works client-side.

With server-side processing every page, sort and search request is sent to Backdrop's own Views Ajax callback (`views/ajax`). Views therefore performs the access check of the display, applies contextual arguments and exposed filters, and the rows are rendered by the same code as the normal table (combined columns, excluded fields, row classes).

#### Notes:

- Set **Caching: None** on the display. Server-side requests ignore the Views cache.
- Sorting uses the `click_sort()` of the field handlers, so only columns that are "sortable" in the style settings and click-sortable can be ordered.
- Global search (search box) and column filters are applied with SQL `LIKE` /`=` on the real database column of each field; see *Search with server-side processing* below. Columns that have no such column (global fields, aggregated fields, formulas, links) are not searched.
- At most 1000 rows are returned per request; "All" is not offered in the length menu.
- The exposed filters of the view keep working: submitting the exposed form re-renders the view through Views Ajax, and the new filter values are sent along with every following DataTables request.
- The style option *Hide empty column* setting is not available (and ignored) with server-side processing : which cells are empty depends on the page, but the columns of a DataTable are defined once.

#### Search with server-side processing (AJAX)

With server-side processing the search box and the text column filters are
evaluated by the database. Columns that store a key or an id and show a label
are searched by what the table shows, so the result is the same as without
AJAX:

* the content type (its name, not the machine name; unless the field is set to
  show the machine name),
* list fields (the labels of the allowed values),
* yes/no fields such as *Content: Published* (the text the field is set up to show, e.g. "Published" / "Not published"; the drop-down filter of such a column lists these texts too),
* taxonomy term and entity reference fields (the names / labels of the
  referenced entities that the user may view; the first 200 matches are used).

For other columns the stored value is searched, so a column whose text is made
by rewriting the result, a formatter or a date format is searched by the
stored value, not by the displayed text. Columns that cannot be searched in SQL
(aggregated fields, formulas, global fields) are left out; the view settings
list them under "Enable the search filter box".

#### Exporting all rows with server-side processing

With server-side processing the table only holds the rows of the current page. Unless disabled, the Export buttons (Copy, CSV, Excel, PDF, Print) therefore ask the server for **all rows that match the current search, column filters and sort order**, export them, and show the previous page again.

- Style option *Export all rows (server-side processing only)* (Buttons section, enabled by default). When disabled, only the visible page is exported.
- Style option *Maximum number of rows to export* (default 10000). The limit is enforced on the server. If more rows match, the first ones are exported and the user gets a notice. Keep it moderate: PDF files in particular get slow and large.
- Client-side tables always export all matching rows; nothing changes there.

#### AJAX history (table state in the URL)

When the View display has *AJAX history* enabled (Advanced settings, next to *Use AJAX*), a server-side DataTable keeps its state in the address bar, so a copied URL opens the same table: the search, the column filters, the sort order, the page, the page length and the visible columns. The parameters are `dt_q` (search), `dt_s` (sort, e.g. `title:asc,type:desc`), `dt_p` (page), `dt_l` (page length), `dt_c` (the visible columns, as chosen with the *Columns* button) and `dt_f[field]` (column filter), and only appear when they differ from the defaults. The address bar is updated in place; no browser history entries are added. The parameters of exposed filters are handled by Views as before. Use only one DataTable with AJAX history per page.

### Scroller

The *Scroller* pagination style only draws the visible portion of the table (and a bit to either side to make the scrolling smooth), while the scrolling container gives the visual impression that the whole table is visible. The related option *Vertical height* (pagination style *Scroller*) takes a number of pixels or any CSS length, e.g. `400px` or `calc(100vh - 300px)`. With the value `fill` the table body uses all the height of the window down to its bottom; `fill 40` keeps 40 pixels free below the table (default 20), e.g. for a footer. The height is measured in the browser and calculated again when the window is resized or the layout of the table changes. It is at least 120 pixels high.

### Column filters

Under *Column filters* in the style settings each column can get a filter in a second header row: a **text box**, or a **drop-down with the distinct values** of the column (similarly as in the Views Selective Filters module).

- Client-side processing: the values are read from the table.
- Server-side processing: the values are read from the database with the query of the view itself (as a derived table), so they work for any Views base table. With *Only list values that are still possible* the list contains only values that exist in the rows left by the exposed filters, contextual arguments, the other column filters and the search box. The column's own filter is ignored, so the user can select another option.
- If a column has more distinct values than the configured maximum, a text box is shown instead.
- Searchable and filterable are columns that read a plain database column and Field API fields (their main column, joined only when a search or filter on it is requested). With server-side processing, columns that cannot be filtered in SQL (aggregated fields, formulas, global fields) get no filter control. Links (edit, delete, ...), drop buttons, the "has new content" marker and the bulk operations checkboxes have nothing to filter and never get a filter, in both modes; they are not listed in the *Column filters* settings and are left out of the search.
- Values are the raw database values (dates are shown formatted, markup is stripped from the labels). To show other labels (e.g. names for ids) implement `hook_datatables_filter_options_alter(&$options, $view, $field_id)`, where `$options` is an array `value => label`.
- Fields with several values: if a Field API field is set to "Display all values in the same row" (as unordered list, ordered list or with a separator), the drop-down offers every single value as its own option, and choosing one shows all rows that contain this value. With server-side processing the values are read from the field table; without it the cell is split according to the field's list type or separator. Rows are never repeated when several values of a row match. A value that itself contains the separator string cannot be told apart in client-side mode; use a list type or another separator in that case.

Filters of hidden columns are not shown.

#### Operators in column filters

The text box column filters accept an operator at the start of the input:

| Input                                | Meaning                                                                    |
| ------------------------------------ | -------------------------------------------------------------------------- |
| `>2025`, `>=2025`, `<2025`, `<=2025` | greater / less than (or equal)                                             |
| `=2025`, `!=2025` (also `<>`)        | exact match / not equal                                                    |
| `!abc`                               | text only: does not contain "abc" (for dates and numbers the same as `!=`) |
| `2020 - 2023` or `2020..2023`        | range (inclusive); the dash needs spaces around it                         |

* **Dates** are compared independently of the display format. Accepted
  values: `2025`, `2025-03`, `2025-03-14`, `2025-03-14 12:30`, `14.3.2025`,
  `3.2025`. `=2025` matches the whole year, `>2025` means after the year 2025.
  Comparison uses the user's time zone.
* **Numbers** accept a dot or a comma as decimal separator.
* **Text** columns support `=`, `!=` and the comparison operators as string
  comparisons (alphabetical, not natural number order). A text stands for
  everything that starts with it, like a year stands for all its days: `<=B`
  includes "Banana", `>B` does not. A dash in text is just a normal "contains"
  search.
* **Columns of custom tables** (for example with views_exposed_table) that are
  shown with a plain Views field handler are recognized by their database type:
  DATETIME, TIMESTAMP and DATE columns get the date operators (the stored value
  is compared as written, without time zone conversion), integer and decimal
  columns get the number operators.
* An incomplete or unparsable operand (for example `>` or `>abc`) is ignored.
* Date columns of Backdrop (created, changed, ...) and Field API date fields
  (Date, Date (ISO format), Date (Unix timestamp)) are supported. Dates are read in
  the time zone the field is shown in (user/site time zone, UTC, or the stored
  wall clock time for the time zone handling "none" or "date" - for the latter
  the time zone of each value is not taken into account). Only the start date
  of a date range field is compared.
* The tooltip of a filter box lists only the operators that work for its column;
  columns the operators cannot be used on have no hint.
* Works in client-side and server-side mode.

### Responsive behaviour

The table is always responsive: when it is too wide for its container (also while the window is being resized), DataTables hides columns, and a click on a row shows the hidden values below it. Which columns go first is derived from the per-column *Responsive priority* of the table style settings (the same setting as for the regular Views table):

- High (default): hidden last.
- Medium: hidden before High columns.
- Low: hidden first.

Among columns of the same priority the right-most is hidden first. A column set to "Expandable" in the *Hidden columns* settings is always shown in the detail row.

### Dates and file sizes sorting

Client-side sorting uses the raw value of the date (written to the cells as `data-order`), so it is independent of the date format. Formatted file sizes (the *File: Size* field) are sorted by the number of bytes in the same way. 

### Bulk operations

A *Content: Bulk operations* field (checkbox in every row and a form to run an action on the selected rows) works in a DataTable, with and without server-side processing.

- The column has no sorting, search or filter, it is never hidden by the responsive behaviour, and it is not part of exports or the *Columns* menu.
- The checkbox in the header selects or clears the rows of the current page.
- The selection is kept when the page changes (also when the rows of a page come from the server), and the action is applied to all selected rows, also those on other pages.
- With server-side processing the form is not built with these rows. When it is submitted, the selected ids are checked against the view again (with its arguments, exposed filters and access checks), and ids that the view would not show are ignored.
- *Select all items on all pages* is not offered in a DataTable, as it could not know the rows that the table filters leave.

### Sticky header

The style option *Sticky table header* keeps the column titles and the filterrow at the top of the window while the page scrolls. It uses CSS(`position: sticky`) on the real header, not Backdrop's cloned header, andrespects the administration bar (elements with a `data-offset-top` attribute)and any `<header>` element of the theme that is fixed or sticky at the top ofthe window (its current height is followed while scrolling, so shrinkingheaders work). Other fixed bars, e.g. a `div`, can be named in the style option*Fixed elements above the sticky header* as a CSS selector.It is not active in the Views preview or together with a fixed table height(Scroller), where the header does not scroll away anyway. The theme must notset `overflow` on a parent element of the table, as that disables stickypositioning.

The filter row is not part of exports (copy, CSV, Excel, PDF, print): only the column titles are exported as header.

### Switching between Table and DataTable

When the display style of a View is switched between the core *Table* and *DataTable*, the options both styles share (columns, sorting, aliases, separators, sticky header, caption, summary, empty text) are kept. The DataTables specific options (elements, layout, paging, column filters and so on) are kept aside while the View uses the core Table and are restored when switching back, so both styles can be tried without losing any settings.

### Debugging SQL queries

The style option *Debug: show SQL queries in the browser console* (under the elements) sends the SQL queries of every server-side table request to the browser, where they are written to the JavaScript console as a collapsed group with the time each query took (values filled in, cache and session queries left out). This shows the final query with column filters, search, ordering and paging, the count queries and the queries for the filter lists. Only users with the permission *Administer views* receive the queries; for everybody else nothing is sent. Switch the option off when it is not needed.

### Export buttons and options for developers

With the style option *Buttons* enabled, the **Export** menu offers Copy, CSV, Excel, PDF and Print. In contrast to DataTables' default, the page title is not written by default as first row/line to the Copy and Excel output.

Modules can change the buttons with `hook_datatables_export_buttons_alter()` (see `datatables.api.php`). The hook receives the button definitions keyed by `copy`, `csv`, `excel`, `pdf` and `print`; every value holds the options documented for that button, e.g. at [https://datatables.net/reference/button/excel](https://datatables.net/reference/button/excel). Unset a key to remove a button, add a key to offer another one.

```php
function mymodule_datatables_export_buttons_alter(array &$buttons, $view) {    
  if ($view->name == 'my_table') {    
    $buttons['pdf']['orientation'] = 'landscape';    
    $buttons['excel']['title'] = '*';  // page title as first row again    
    $buttons['excel']['filename'] = 'my-table';    
  }    
}
```

Options that are JavaScript functions (e.g. `customize`) cannot be written in PHP. Give the name of a global function with the prefix `js:`, e.g. `$buttons['pdf']['customize'] = 'js:Backdrop.myModule.pdfCustomize';`. The function must exist when the table is built, for instance in a file added with `backdrop_add_js()` and `'group' => JS_LIBRARY`. This works for every option of the table, not only for buttons.

`hook_datatables_options_alter()` receives all options of a table (columns, paging, language, buttons, ...) to change anything else.

### Views preview

The preview of the Views editor is reloaded after every change and always works client-side, even when server-side processing is enabled. To keep it fast it only loads the number of rows set under*Pagination and Page Length: Rows in the Views preview* (default 100, 0 = all); a caption tells when rows were left out. The real page is not affected. If itis still too slow, switch off *Auto preview* below the view settings.

## Differences from Drupal 7

- In contrast to the Drupal module, which used DataTables v1.9 from 2012, the Backdrop module uses DataTables v2.x.
- The DataTables library and used extensions are already packaged with this module and do not need to be installed separately.
- DataTables requires jQuery 1.7 or newer which is already included in Backdrop.

## DataTables library and extensions

- DataTables version 2.3.8 and the following extensions are included:
- Buttons 3.2.6 (successor of TableTools)
- Responsive 3.0.8

## Issues

Bugs and feature requests should be reported in [the Issue Queue](https://github.com/backdrop-contrib/foo-project/issues).

## Current Maintainers

- [Alex Höbart](https://github.com/AlexHoebart-ICPDR)

## Credits

- Ported to Backdrop CMS by [Alex Höbart](https://github.com/AlexHoebart-ICPDR).
- Originally written for Drupal by [Jon Duell](https://www.drupal.org/u/duellj), [dqd](https://www.drupal.org/u/dqd), [Mitch Portier (Arkener)](https://www.drupal.org/u/arkener).
- Based on DataTables written by [Allan Jardine/SpryMedia](https://sprymedia.co.uk/about).
- Further developed with AI assistance.

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text.

The DataTables library is available under the [MIT license](https://www.datatables.net/license/mit).

## 

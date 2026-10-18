# DataTables

This module provides Views integration for the DataTables library, which provides advanced interaction controls to HTML tables such as dynamic pagination, on-the-fly filtering, and column sorting.

Version 2.1 of this module introduces two useful features of DataTables:

- **Server-side processing** which draws each table page using an Ajax request to the server and therefore is useful to show large data sets.
- **Scroller** which draws only the visible portion of the table (and a bit to either side to make the scrolling smooth), while the scrolling container gives the visual impression that the whole table is visible.

Both features can be enabled and used independently but they naturally fit well together. See a DataTables example with both features at [https://datatables.net/extensions/scroller/examples/initialisation/server-side_processing.html](https://datatables.net/extensions/scroller/examples/initialisation/server-side_processing.html).

For full documentation and examples, visit the DataTables website: [https://datatables.net/](https://datatables.net/).

## Installation

- Install this module using the [official Backdrop CMS instructions](([https://docs.backdropcms.org/documentation/extend-with-modules](https://docs.backdropcms.org/documentation/extend-with-modules)).
- There are two ways you can use DataTables:

1. Create a View and use the DataTables display format.
2. Use theme_datatable function in your own module code. Detaileddocumentation is included with the function.


## Server-side processing

Whether DataTables works in the browser or asks the server for every page isderived from the Views settings:

- **Display uses AJAX** (Advanced settings: *Use AJAX*) and the paginationstyle is not *No Pagination*: **server-side processing**.
- Otherwise: all rows are loaded at once and DataTables works client-side.

With server-side processing every page, sort and search request is sent toBackdrop's own Views Ajax callback (`views/ajax`).Views therefore performs the access check of the display, applies contextualarguments and exposed filters, and the rows are rendered by the same code asthe normal table (combined columns, excluded fields, row classes).

### Notes:

- Set **Caching: None** on the display. Server-side requests ignore the Viewscache.
- Sorting uses the `click_sort()` of the field handlers, so only columns thatare "sortable" in the style settings and click-sortable can be ordered.
- Global search (search box) and column filters are applied with SQL `LIKE` /`=` on the real database column of each field. Columns that have no suchcolumn (global fields, Field API fields, custom text) are not searched.
- At most 1000 rows are returned per request; "All" is not offered in thelength menu.
- The exposed filters of the view keep working: submitting the exposed formre-renders the view through Views Ajax, and the new filter values are sentalong with every following DataTables request.

### Views preview

The preview of the Views editor is reloaded after every change and alwaysworks client-side. To keep it fast it only loads the number of rows set under*Pagination and Page Length: Rows in the Views preview* (default 100, 0 = all);a caption tells when rows were left out. The real page is not affected. If itis still too slow, switch off *Auto preview* below the view settings.

## Column filters

Under *Column filters* in the style settings each column can get a filter in asecond header row: a **text box**, or a **drop-down with the distinct values**of the column (similarly as in the Views Selective Filters module).

- Client-side processing: the values are read from the table.
- Server-side processing: the values are read from the database with thequery of the view itself (as a derived table), so they work for any Viewsbase table. With *Only list values that are stillpossible* the list contains only values that exist in the rows left by theexposed filters, contextual arguments, the other column filters and thesearch box. The column's own filter is ignored, so the user can select another option.
- If a column has more distinct values than the configured maximum, a textbox is shown instead.
- Searchable and filterable are columns that read a plain database column andField API fields (their main column, joined only when a search or filter onit is requested). With server-side processing, columns that cannot befiltered in SQL (aggregated fields, formulas, global fields) get no filtercontrol.
- Values are the raw database values (dates are shown formatted, markup isstripped from the labels). To show other labels (e.g. names forids) implement `hook_datatables_filter_options_alter(&$options, $view, $field_id)`, where `$options` is an array `value => label`.
- Fields with several values: if a Field API field is set to "Display allvalues in the same row" (as unordered list, ordered list or with aseparator), the drop-down offers every single value as its own option, andchoosing one shows all rows that contain this value. With server-sideprocessing the values are read from the field table; without it the cellis split according to the field's list type or separator. Rows are neverrepeated when several values of a row match. A value that itself containsthe separator string cannot be told apart in client-side mode; use a listtype or another separator in that case.

Filters of hidden columns are not shown.

## Sticky header

The style option *Sticky table header* keeps the column titles and the filterrow at the top of the window while the page scrolls. It uses CSS(`position: sticky`) on the real header, not Backdrop's cloned header, andrespects the administration bar (elements with a `data-offset-top` attribute)and any `<header>` element of the theme that is fixed or sticky at the top ofthe window (its current height is followed while scrolling, so shrinkingheaders work). Other fixed bars, e.g. a `div`, can be named in the style option*Fixed elements above the sticky header* as a CSS selector.It is not active in the Views preview or together with a fixed table height(Scroller), where the header does not scroll away anyway. The theme must notset `overflow` on a parent element of the table, as that disables stickypositioning.

The filter row is not part of exports (copy, CSV, Excel, PDF, print): only thecolumn titles are exported as header.

## Responsive behavior

The table is always responsive: when it is too wide for its container (also while the window is being resized), DataTables hides columns, and a click on a row shows the hidden values below it. Which columns go first is derived from the per-column *Responsive priority* of the table style settings (the same setting as for the regular Views table):

- High (default): hidden last.
- Medium: hidden before High columns.
- Low: hidden first.

Among columns of the same priority the right-most is hidden first. A column set to "Expandable" in the *Hidden columns* settings is always shown in the detail row.

## Dates and alignment

Client-side sorting uses the raw value of the date (written to the cells as `data-order`), so it is independent of the date format. The alignment of the style settings applies.

## Differences from Drupal 7
- In contrast to the Drupal module, which used DataTables v1.9 from 2012, theBackdrop module uses DataTables v2.x.
- The DataTables library and used extensions are already packaged with this module and do not need to be installed separately.
- DataTables requires jQuery 1.7 or newer which is already included in Backdrop.

## DataTables library and extensions

- DataTables version 2.3.8 and the following extensions are included:
- Buttons 3.2.6 (successor of TableTools)
- Responsive 3.0.8

## Issues

Bugs and feature requests should be reported in [the Issue Queue](https://github.com/backdrop-contrib/foo-project/issues).

## Current Maintainers

- [Alex Höbart](https://github.com/AlexHoebart-ICPDR).
- Seeking additional maintainers.

## Credits

- Ported to Backdrop CMS by [Alex Höbart](https://github.com/AlexHoebart-ICPDR).
- Originally written for Drupal by [Jon Duell](https://www.drupal.org/u/duellj), [dqd](https://www.drupal.org/u/dqd), [Mitch Portier (Arkener)](https://www.drupal.org/u/arkener).
- Based on DataTables written by [Allan Jardine/SpryMedia](https://sprymedia.co.uk/about).

## License

This project is GPL v2 software. See the LICENSE.txt file in this directory for complete text. 

The DataTables library is available under the [MIT license](https://www.datatables.net/license/mit).

 

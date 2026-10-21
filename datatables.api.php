<?php

/**
 * @file
 * Hooks provided by the DataTables module.
 */

/**
 * Alters the choices of a drop-down column filter.
 *
 * @param array $options
 *   The distinct values of the column, as value => label. Labels default to the
 *   value. The order is kept.
 * @param view $view
 *   The view that is displayed.
 * @param string $field_id
 *   The id of the field of the column.
 */
function hook_datatables_filter_options_alter(array &$options, $view, $field_id) {
  if ($view->name == 'my_table' && $field_id == 'country_id') {
    foreach ($options as $id => $label) {
      $options[$id] = my_module_country_name($id);
    }
  }
}

/**
 * Alters the buttons of the "Export" menu of a table.
 *
 * The buttons are configured with the options documented at
 * https://datatables.net/extensions/buttons/. Option values that are
 * JavaScript functions (like "customize") cannot be written in PHP: give the
 * name of a global function (or of a function in a global object, as in
 * Backdrop.myModule.pdfCustomize) with the prefix "js:" and the JavaScript
 * of this module resolves it before the table is created. Such a function
 * must be defined before the behaviors are attached, e.g. in a script added
 * with backdrop_add_js() and the 'group' => JS_LIBRARY.
 *
 * @param array $buttons
 *   The button definitions keyed by name: copy, csv, excel, pdf, print. Unset
 *   a key to remove a button; add keys to offer more buttons.
 * @param view $view
 *   The view that is displayed.
 */
function hook_datatables_export_buttons_alter(array &$buttons, $view) {
  if ($view->name == 'my_table') {
    // PDF in landscape format, A3 paper.
    $buttons['pdf']['orientation'] = 'landscape';
    $buttons['pdf']['pageSize'] = 'A3';
    // Own file name; keep the page title as title of the Excel sheet.
    $buttons['excel']['filename'] = 'my-table';
    $buttons['excel']['title'] = '*';
    // Change the PDF document with a JavaScript function.
    $buttons['pdf']['customize'] = 'js:Backdrop.myModule.pdfCustomize';
    // No printing.
    unset($buttons['print']);
  }
}

/**
 * Alters the DataTables options of a table.
 *
 * Called last, with every option of the table including those set from the
 * style settings. The key "backdrop" holds settings of this module's own
 * JavaScript. For function values see hook_datatables_export_buttons_alter().
 *
 * @param array $options
 *   The options as they are passed to DataTables (columns, buttons, ...).
 * @param view $view
 *   The view that is displayed.
 */
function hook_datatables_options_alter(array &$options, $view) {
  if ($view->name == 'my_table') {
    $options['pageLength'] = 50;
    $options['language']['search'] = t('Find:');
    // Move the elements around with the "layout" option (not set when the
    // deprecated "dom" option of the style is used), e.g. buttons at the left
    // and the information below the table at the right.
    // https://datatables.net/reference/option/layout
    $options['layout']['topStart'] = 'buttons';
    $options['layout']['topEnd'] = 'search';
    $options['layout']['bottomStart'] = 'paging';
    $options['layout']['bottomEnd'] = 'info';
  }
}

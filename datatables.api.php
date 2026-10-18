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

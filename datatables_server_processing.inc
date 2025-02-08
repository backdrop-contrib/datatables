<?php

/*
 * DataTables server-side processing script.
 *
 * See https://datatables.net/usage/server-side for full details on the server-
 * side processing requirements of DataTables.
 *
 */

function datatables_server_processing($view_name, $display_name = 'page') {

  // Get the view.
  $view = views_get_view($view_name);
  $view->set_display($display_name);

  // Set tag, see datatables_views_pre_build
  $view->tag = 'datatables_server_processing';

  // Get request
  $request = $_GET;
  // Avoid TypeError: Illegal offset type in isset or empty in views_plugin_style_table->build_sort() (line 53 of core\modules\views\plugins\views_plugin_style_table.inc).
  if (isset($_GET['order'])) {
    unset($_GET['order']);
    // Put datatables order parameter back under new name
    $_GET['dt_order'] = $request['order'];
  }
  if (isset($request['query'])) {
    parse_str(substr($request['query'],1), $parms);
  }

  $view->pre_execute();
  $view->set_exposed_input($parms);
  $view->init_query();

  // Get search filter (can be separated by space)
  $strings = [];
  if ( isset($request['search']) && $request['search']['value'] != '' ) {
    $strings = explode(' ', $request['search']['value']);
  }

  // Construct query conditions
  $idx = 2;
  foreach ($strings as $string) {
    $where = ['type' => 'OR'];
    foreach ($request['columns'] as $c =>$col) {
      $field = $view->field[$col['data']];
      if ($field->table == 'views') continue; // skip pseudo-columns
      $where['conditions'][] = array(
        'field' => $field->table . '.' . $field->real_field,
        'value' => '%' . trim($string) . '%',
        'operator' => 'LIKE'
      );
    }
    $view->query->where[$idx] = $where;
    $idx++;
  }

  // Get total results
  $view->execute();
  $records_total = $request['start'] + $view->total_rows;

  // Render raw values to field values
  $data = $view->style_plugin->render_fields($view->result);

  // Output
  $output = array(
    "draw"            => isset ( $request['draw'] ) ?
      intval( $request['draw'] ) :
      0,
    "recordsTotal"    => intval( $records_total ),
//TODO: get total count when filtering
    "recordsFiltered" => intval( $records_total ),
    "data"            => $data
  );

  echo json_encode($output);
}

<?php

/**
 * @file
 * Server-side processing for the DataTables style plugin.
 *
 * Instead of having a private menu callback that re-implements a View
 * (and bypasses its access settings), DataTables requests are sent to
 * Backdrop's own Views Ajax callback (views/ajax). Views therefore does
 * everything it normally does for an Ajax View: access check, display
 * selection, contextual arguments, exposed input, destination handling.
 *
 * This file only contains what is specific to DataTables:
 *  - parsing/validating the DataTables request,
 *  - adding the DataTables search/column filters/paging to the built query,
 *  - computing the lists of distinct values for "select" column filters.
 *
 * The request travels in $_POST['datatables'] and is picked up in
 * datatables_views_pre_view(). The answer is a custom Ajax command
 * ('datatablesResponse') created in datatables_views_ajax_data_alter().
 */

/**
 * Maximum number of rows returned for one server-side request.
 */
define('DATATABLES_SERVER_MAX_LENGTH', 1000);

/**
 * Parses and sanitizes the raw DataTables request.
 *
 * @param array $raw
 *   The raw value of $_POST['datatables'].
 *
 * @return array
 *   A request array. Everything in it is safe to use in the code below.
 */
function datatables_server_parse_request(array $raw) {
  $string = function ($value) {
    return is_scalar($value) ? backdrop_substr(trim((string) $value), 0, 255) : '';
  };
  $field_id = function ($value) {
    return (is_string($value) && preg_match('/^[A-Za-z0-9_]+$/', $value)) ? $value : NULL;
  };

  $request = array(
    'dom_id' => isset($raw['dom_id']) ? preg_replace('/[^a-zA-Z0-9_-]+/', '-', (string) $raw['dom_id']) : '',
    'draw' => isset($raw['draw']) ? (int) $raw['draw'] : 0,
    'start' => isset($raw['start']) ? max(0, (int) $raw['start']) : 0,
    'length' => isset($raw['length']) ? (int) $raw['length'] : 10,
    'search' => '',
    'columns' => array(),
    'order' => array(),
    'options' => array(),
    // The request is an export of all rows, see datatables_views_pre_view().
    'export' => !empty($raw['export']),
  );
  // DataTables sends -1 for "all records". That defeats the purpose of
  // server-side processing, so the page length is limited.
  if ($request['length'] < -1 || $request['length'] == 0) {
    $request['length'] = 10;
  }
  if ($request['length'] == -1 || $request['length'] > DATATABLES_SERVER_MAX_LENGTH) {
    $request['length'] = DATATABLES_SERVER_MAX_LENGTH;
  }

  if (isset($raw['search']['value'])) {
    $request['search'] = $string($raw['search']['value']);
  }

  if (!empty($raw['columns']) && is_array($raw['columns'])) {
    foreach ($raw['columns'] as $idx => $col) {
      $id = isset($col['data']) ? $field_id($col['data']) : NULL;
      $request['columns'][(int) $idx] = array(
        'field' => $id,
        'searchable' => !isset($col['searchable']) || $col['searchable'] === TRUE || $col['searchable'] === 'true' || $col['searchable'] === '1',
        'search' => isset($col['search']['value']) ? $string($col['search']['value']) : '',
      );
    }
  }

  if (!empty($raw['order']) && is_array($raw['order'])) {
    foreach ($raw['order'] as $item) {
      $idx = isset($item['column']) ? (int) $item['column'] : -1;
      if (isset($request['columns'][$idx]) && $request['columns'][$idx]['field']) {
        $request['order'][] = array(
          'field' => $request['columns'][$idx]['field'],
          'dir' => (isset($item['dir']) && strtolower($item['dir']) == 'desc') ? 'desc' : 'asc',
        );
      }
    }
  }

  if (!empty($raw['options']) && is_array($raw['options'])) {
    foreach ($raw['options'] as $id) {
      if ($field_id($id)) {
        $request['options'][] = $id;
      }
    }
  }

  return $request;
}

/**
 * Returns the cleaned exposed input of a view, to be sent along with every
 * DataTables request.
 */
function datatables_server_exposed_input($view) {
  $input = $view->get_exposed_input();
  foreach (array(
    'datatables', 'page', 'q', 'form_build_id', 'form_token', 'form_id', 'op',
    'ajax_html_ids', 'ajax_page_state', 'order', 'sort',
    '_triggering_element_name', '_triggering_element_value',
  ) as $key) {
    unset($input[$key]);
  }
  return $input;
}

/**
 * Finds the database column behind a field handler.
 *
 * Supported are fields that read one plain column of a table (the usual case
 * for custom tables) and Field API fields (their main column, i.e. the
 * "click sort column"). Aggregated fields, formulas and global (views) fields
 * are not supported.
 *
 * @return array|null
 *   array('table_alias' => ..., 'column' => ...) or NULL when the field cannot
 *   be searched and filtered in SQL. For a Field API field the table alias is
 *   NULL as long as its table has not been joined to the query (see
 *   datatables_server_join_field_tables()).
 */
function datatables_server_field_target($handler) {
  if (!($handler instanceof views_handler_field)) {
    return NULL;
  }
  if (!empty($handler->options['group_type']) && $handler->options['group_type'] != 'group') {
    return NULL;
  }
  if ($handler->table == 'views') {
    return NULL;
  }

  if ($handler instanceof views_handler_field_field) {
    if (empty($handler->definition['field_name'])) {
      return NULL;
    }
    $field_name = $handler->definition['field_name'];
    $column = !empty($handler->options['click_sort_column']) ? $handler->options['click_sort_column'] : NULL;
    if (!$column) {
      $info = field_info_field($field_name);
      if (empty($info['columns'])) {
        return NULL;
      }
      reset($info['columns']);
      $column = key($info['columns']);
    }
    return array(
      'table_alias' => !empty($handler->table_alias) ? $handler->table_alias : NULL,
      'column' => _field_sql_storage_columnname($field_name, $column),
    );
  }

  if (empty($handler->table_alias) || empty($handler->real_field) || !preg_match('/^[A-Za-z0-9_]+$/', $handler->real_field)) {
    return NULL;
  }
  if (empty($handler->field_alias) || $handler->field_alias == 'unknown') {
    return NULL;
  }
  return array('table_alias' => $handler->table_alias, 'column' => $handler->real_field);
}

/**
 * Returns "table_alias.column" for a field handler that can be searched and
 * filtered in SQL, or NULL.
 */
function datatables_server_field_sql($handler) {
  $target = datatables_server_field_target($handler);
  return ($target && $target['table_alias']) ? $target['table_alias'] . '.' . $target['column'] : NULL;
}

/**
 * Joins the tables of Field API fields that the request searches or filters.
 *
 * Views does not join the table of a Field API field when it only displays it
 * (the values come from the loaded entities). Must run before the query is
 * built, see datatables_views_query_alter().
 */
function datatables_server_join_field_tables($view) {
  $request = $view->datatables_request;
  $filters = !empty($view->style_plugin->options['filters']) ? $view->style_plugin->options['filters'] : array();
  $need = array();
  if (!empty($view->datatables_exclude)) {
    $need[$view->datatables_exclude] = TRUE;
  }
  foreach ($request['columns'] as $col) {
    $id = $col['field'];
    if (!$id || !isset($view->field[$id])) {
      continue;
    }
    $mode = isset($filters[$id]) ? $filters[$id] : 'none';
    if ($col['search'] !== '' && in_array($mode, array('text', 'select'))) {
      $need[$id] = TRUE;
    }
    if ($request['search'] !== '' && $col['searchable']) {
      $need[$id] = TRUE;
    }
  }
  foreach (array_keys($need) as $id) {
    $handler = $view->field[$id];
    if ($handler instanceof views_handler_field_field && empty($handler->table_alias)) {
      $handler->ensure_my_table();
    }
  }
}

/**
 * Adds DataTables search, column filters and paging to an already built View.
 *
 * Called from hook_views_post_build(). The conditions are added to the final
 * SelectQuery objects (not to the Views WHERE groups) so they are always ANDed
 * with all the View's own filters, whatever its filter group operator is.
 *
 * Roles of the view object:
 *  - 'main':    the View that produces the rows for a draw.
 *  - 'count':   a helper that counts rows without DataTables filters.
 *  - 'options': a helper whose query is used to read distinct values.
 */
function datatables_server_apply_request($view, array $request, $role = 'main', $exclude = NULL) {
  $queries = array();
  foreach (array('query', 'count_query') as $key) {
    if (isset($view->build_info[$key]) && $view->build_info[$key] instanceof SelectQueryInterface) {
      $queries[$key] = $view->build_info[$key];
    }
  }
  if (empty($queries['query'])) {
    return;
  }

  $filters = !empty($view->style_plugin->options['filters']) ? $view->style_plugin->options['filters'] : array();

  // Resolve the columns once.
  $global = array();
  $column_conditions = array();
  foreach ($request['columns'] as $col) {
    $id = $col['field'];
    if (!$id || !isset($view->field[$id])) {
      continue;
    }
    $sql = datatables_server_field_sql($view->field[$id]);
    if (!$sql) {
      continue;
    }
    if ($col['searchable']) {
      $global[] = $sql;
    }
    // Column filters come from the style options, never from the request.
    $mode = isset($filters[$id]) ? $filters[$id] : 'none';
    if ($col['search'] !== '' && in_array($mode, array('text', 'select')) && $id !== $exclude) {
      $column_conditions[] = array($sql, $col['search'], $mode);
    }
  }
  $tokens = ($request['search'] === '') ? array() : preg_split('/\s+/', $request['search']);

  foreach ($queries as $query) {
    // Global search: every word must match at least one searchable column.
    if ($role != 'count' && $global) {
      foreach ($tokens as $token) {
        $or = db_or();
        foreach ($global as $sql) {
          $or->condition($sql, '%' . db_like($token) . '%', 'LIKE');
        }
        $query->condition($or);
      }
    }
    if ($role != 'count') {
      foreach ($column_conditions as $c) {
        if ($c[2] == 'select') {
          $query->condition($c[0], $c[1]);
        }
        else {
          $query->condition($c[0], '%' . db_like($c[1]) . '%', 'LIKE');
        }
      }
    }
  }

  // A Field API field with several values per row ("Display all values in the
  // same row") that was joined above would repeat the row once for each value
  // that matches. Showing every row once is what the table promises.
  if ($role != 'options') {
    $multiple_joined = FALSE;
    foreach ($view->field as $handler) {
      if ($handler instanceof views_handler_field_field && !empty($handler->multiple) && !empty($handler->options['group_rows']) && !empty($handler->table_alias)) {
        $multiple_joined = TRUE;
      }
    }
    if ($multiple_joined) {
      foreach ($queries as $query) {
        $query->distinct();
      }
    }
  }

  if ($role == 'main') {
    if ($request['length'] > 0) {
      $queries['query']->range($request['start'], $request['length']);
    }
    else {
      // All records requested.
      $queries['query']->range();
    }
  }
  elseif ($role == 'count') {
    $queries['query']->range(0, 1);
  }
}

/**
 * Creates a copy of the view for helper queries (counts, distinct values).
 */
function datatables_server_helper_view($view, array $request, $role, $exclude = NULL) {
  $helper = views_get_view($view->name);
  if (!$helper || !$helper->set_display($view->current_display)) {
    return NULL;
  }
  $helper->set_arguments($view->args);
  $helper->set_exposed_input($view->get_exposed_input());
  $helper->datatables_request = $request;
  $helper->datatables_role = $role;
  $helper->datatables_exclude = $exclude;
  $helper->get_total_rows = ($role == 'count');
  return $helper;
}

/**
 * Number of rows of the View before the DataTables filters are applied
 * (DataTables' "recordsTotal"). Exposed filters of the View still apply.
 */
function datatables_server_total_count($view, array $request) {
  $base = $request;
  $base['search'] = '';
  foreach ($base['columns'] as $i => $col) {
    $base['columns'][$i]['search'] = '';
  }
  $column_searches = array_filter(array_map(function ($c) {
    return $c['search'];
  }, $request['columns']), function ($value) {
    return $value !== '';
  });
  if ($request['search'] === '' && !$column_searches) {
    // No DataTables filter active: same as the filtered count.
    return (int) $view->total_rows;
  }
  $helper = datatables_server_helper_view($view, $base, 'count');
  if (!$helper) {
    return (int) $view->total_rows;
  }
  $helper->execute();
  return (int) $helper->total_rows;
}

/**
 * Lists of distinct values for the "select" column filters.
 *
 * Like views_selective_filters, the values are the ones present in the rows
 * that are left after all other filters (exposed filters, contextual
 * arguments, other column filters, global search) have been applied. The
 * column's own filter is ignored so the user can switch to another value.
 *
 * @return array
 *   Keyed by field id: array('values' => array(array('value' =>, 'label' =>)),
 *   'truncated' => bool).
 */
function datatables_server_filter_options($view, array $request) {
  $result = array();
  $style_options = $view->style_plugin->options;

  foreach ($request['options'] as $id) {
    if (empty($style_options['filters'][$id]) || $style_options['filters'][$id] != 'select' || !isset($view->field[$id])) {
      continue;
    }
    $limit = !empty($style_options['filter_options']['limit']) ? (int) $style_options['filter_options']['limit'] : 100;
    $cascade = !empty($style_options['filter_options']['cascade']);

    // When not cascading, use the request without any DataTables filter.
    $helper_request = $request;
    if (!$cascade) {
      $helper_request['search'] = '';
      foreach ($helper_request['columns'] as $i => $col) {
        $helper_request['columns'][$i]['search'] = '';
      }
    }

    $helper = datatables_server_helper_view($view, $helper_request, 'options', $id);
    if (!$helper || !$helper->build() || empty($helper->build_info['query']) || !isset($helper->field[$id])) {
      continue;
    }
    $target = datatables_server_field_sql($helper->field[$id]);
    $inner = $helper->build_info['query'];
    if (!$target || !($inner instanceof SelectQueryInterface)) {
      continue;
    }
    list($target_table, $target_column) = explode('.', $target, 2);

    // Reuse the View's query as a derived table: this keeps every join,
    // filter and relationship of the View without knowing anything about
    // them (works for any base table, including custom tables). The column
    // that is filtered is added to its select list under a known name.
    $order = &$inner->getOrderBy();
    $order = array();
    unset($order);
    $inner->range();
    $inner->addField($target_table, $target_column, 'dt_filter_value');
    $inner->addMetaData('view', $helper);
    $base_data = views_fetch_data($helper->base_table);
    if (empty($helper->query->options['disable_sql_rewrite']) && isset($base_data['table']['base']['access query tag'])) {
      $inner->addTag($base_data['table']['base']['access query tag']);
    }
    $inner->preExecute();

    $outer = db_select($inner, 'dt_inner');
    $outer->addField('dt_inner', 'dt_filter_value', 'dt_value');
    $outer->distinct();
    $outer->isNotNull('dt_inner.dt_filter_value');
    $outer->orderBy('dt_value');
    $outer->range(0, $limit + 1);

    try {
      $values = $outer->execute()->fetchCol();
    }
    catch (Exception $e) {
      watchdog_exception('datatables', $e);
      continue;
    }

    $values = array_values(array_filter($values, function ($v) {
      return $v !== '';
    }));
    $truncated = count($values) > $limit;
    if ($truncated) {
      $values = array_slice($values, 0, $limit);
    }

    // Value => label. Modules may replace labels (e.g. ids by names).
    $labels = array();
    $handler = $helper->field[$id];
    foreach ($values as $value) {
      $label = (string) $value;
      if (strpos($label, '<') !== FALSE) {
        // Markup (e.g. text fields with formatting) is not useful in a list.
        $plain = trim(preg_replace('/\s+/', ' ', decode_entities(strip_tags($label))));
        if ($plain !== '') {
          $label = $plain;
        }
      }
      $label = truncate_utf8($label, 100, TRUE, TRUE);
      if ($handler instanceof views_handler_field_date) {
        // Show dates the way the table shows them.
        $row = new stdClass();
        $row->{$handler->field_alias} = $value;
        $formatted = trim(strip_tags((string) $handler->render($row)));
        if ($formatted !== '') {
          $label = $formatted;
        }
      }
      $labels[$value] = $label;
    }
    backdrop_alter('datatables_filter_options', $labels, $view, $id);

    $list = array();
    foreach ($labels as $value => $label) {
      $list[] = array('value' => (string) $value, 'label' => (string) $label);
    }
    $result[$id] = array('values' => $list, 'truncated' => $truncated);
  }

  return $result;
}

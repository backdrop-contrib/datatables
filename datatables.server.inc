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
    'dt_q', 'dt_s', 'dt_p', 'dt_l', 'dt_c', 'dt_f',
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
 * Tells whether a field can be searched and filtered in SQL.
 *
 * Like datatables_server_field_target(), but can be used before the query has
 * been built (the settings form), when the handlers have no table alias yet.
 */
function datatables_server_field_searchable($handler) {
  if (!($handler instanceof views_handler_field)) {
    return FALSE;
  }
  if (!empty($handler->options['group_type']) && $handler->options['group_type'] != 'group') {
    return FALSE;
  }
  if ($handler->table == 'views') {
    return FALSE;
  }
  if ($handler instanceof views_handler_field_field) {
    return !empty($handler->definition['field_name']);
  }
  return !empty($handler->real_field) && preg_match('/^[A-Za-z0-9_]+$/', $handler->real_field);
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
 * Reads an operator expression from the text of a column filter.
 *
 * Supported: "> x", ">= x", "< x", "<= x", "= x", "!= x" (also "<>"), "! x" (does not contain), and the
 * range "a - b" (dash with spaces) or "a..b". Everything else is a plain
 * "contains" search.
 *
 * @return array|null
 *   NULL if the text is no expression. Otherwise array('op' => ..., 'a' =>
 *   first value or NULL while the value is still being typed, 'b' => second
 *   value of a range) where op is one of >, >=, <, <=, =, !=, ! (does not
 *   contain), between.
 */
function datatables_filter_parse($text) {
  $text = trim($text);
  $m = array();
  if (preg_match('/^(>=|<=|<>|!=|>|<|=|!)\s*(.*)$/s', $text, $m)) {
    $op = ($m[1] == '<>') ? '!=' : $m[1];
    $value = trim($m[2]);
    return array('op' => $op, 'a' => ($value === '') ? NULL : $value, 'b' => NULL);
  }
  if (preg_match('/^(.+?)(?:\s+-\s+|\.\.)(.+)$/s', $text, $m)) {
    return array('op' => 'between', 'a' => trim($m[1]), 'b' => trim($m[2]));
  }
  return NULL;
}

/**
 * Turns a date as typed in a filter into the period it stands for.
 *
 * Accepted: YYYY, YYYY-MM, YYYY-MM-DD, YYYY-MM-DD HH[:MM[:SS]], D.M.YYYY and
 * M.YYYY. The period is the whole year, month, day, ... that was given.
 *
 * @return array|null
 *   array(first timestamp of the period, first timestamp after it) in the
 *   time zone of the user, or NULL if the text is no date.
 */
function datatables_server_parse_date($text, $timezone) {
  $m = array();
  $text = trim($text);
  $parts = NULL;
  if (preg_match('/^(\d{4})(?:-(\d{1,2})(?:-(\d{1,2})(?:[ T](\d{1,2})(?::(\d{2})(?::(\d{2}))?)?)?)?)?$/', $text, $m)) {
    $parts = array(
      'y' => (int) $m[1],
      'm' => isset($m[2]) && $m[2] !== '' ? (int) $m[2] : NULL,
      'd' => isset($m[3]) && $m[3] !== '' ? (int) $m[3] : NULL,
      'h' => isset($m[4]) && $m[4] !== '' ? (int) $m[4] : NULL,
      'i' => isset($m[5]) && $m[5] !== '' ? (int) $m[5] : NULL,
      's' => isset($m[6]) && $m[6] !== '' ? (int) $m[6] : NULL,
    );
  }
  elseif (preg_match('/^(?:(\d{1,2})\.)?(\d{1,2})\.(\d{4})$/', $text, $m)) {
    $parts = array(
      'y' => (int) $m[3],
      'm' => (int) $m[2],
      'd' => $m[1] !== '' ? (int) $m[1] : NULL,
      'h' => NULL, 'i' => NULL, 's' => NULL,
    );
  }
  if (!$parts || $parts['y'] < 1000) {
    return NULL;
  }
  $month = $parts['m'] !== NULL ? $parts['m'] : 1;
  $day = $parts['d'] !== NULL ? $parts['d'] : 1;
  if ($month < 1 || $month > 12 || !checkdate($month, $day, $parts['y'])) {
    return NULL;
  }
  $hour = (int) $parts['h'];
  $minute = (int) $parts['i'];
  $second = (int) $parts['s'];
  if ($hour > 23 || $minute > 59 || $second > 59) {
    return NULL;
  }
  try {
    $zone = new DateTimeZone($timezone);
    $start = new DateTime(sprintf('%04d-%02d-%02d %02d:%02d:%02d', $parts['y'], $month, $day, $hour, $minute, $second), $zone);
  }
  catch (Exception $e) {
    return NULL;
  }
  $step = '+1 year';
  foreach (array('s' => '+1 second', 'i' => '+1 minute', 'h' => '+1 hour', 'd' => '+1 day', 'm' => '+1 month') as $key => $modify) {
    if ($parts[$key] !== NULL) {
      $step = $modify;
      break;
    }
  }
  $next = clone $start;
  $next->modify($step);
  return array($start->getTimestamp(), $next->getTimestamp());
}

/**
 * Builds the SQL condition of an operator expression of a column filter.
 *
 * @param string $sql
 *   "table_alias.column".
 * @param array $parsed
 *   See datatables_filter_parse().
 * @param string $kind
 *   'date', 'number' or 'text', see datatables_column_kind().
 *
 * @return object|null
 *   A condition, or NULL when the value cannot be used (yet), in which case
 *   the filter is ignored.
 */
function datatables_server_operator_condition($sql, array $parsed, $kind, $date_info = NULL) {
  $op = $parsed['op'];
  if ($op == '!' && $kind != 'text') {
    // "Does not contain" means "not equal" for dates and numbers.
    $op = $parsed['op'] = '!=';
  }
  if ($parsed['a'] === NULL || ($op == 'between' && $parsed['b'] === NULL)) {
    return NULL;
  }

  if ($kind == 'date') {
    $date_info = $date_info ? $date_info : array('storage' => 'int', 'zone' => backdrop_get_user_timezone());
    $a = datatables_server_parse_date($parsed['a'], $date_info['zone']);
    $b = ($op == 'between') ? datatables_server_parse_date($parsed['b'], $date_info['zone']) : NULL;
    if (!$a || ($op == 'between' && !$b)) {
      return NULL;
    }
    // The stored format: timestamps or date strings (compared as text, which
    // keeps the order because the strings are fixed length).
    $format = array('int' => NULL, 'datetime' => 'Y-m-d H:i:s', 'iso' => 'Y-m-d\\TH:i:s', 'dateonly' => 'Y-m-d');
    $format = $format[$date_info['storage']];
    if ($format) {
      foreach (array(&$a, &$b) as &$period) {
        if ($period) {
          $period = array(gmdate($format, $period[0]), gmdate($format, $period[1]));
        }
      }
      unset($period);
    }
    switch ($op) {
      case '=':
        return db_and()->condition($sql, $a[0], '>=')->condition($sql, $a[1], '<');

      case '!=':
        return db_or()->condition($sql, $a[0], '<')->condition($sql, $a[1], '>=');

      case '>':
        return db_and()->condition($sql, $a[1], '>=');

      case '>=':
        return db_and()->condition($sql, $a[0], '>=');

      case '<':
        return db_and()->condition($sql, $a[0], '<');

      case '<=':
        return db_and()->condition($sql, $a[1], '<');

      case 'between':
        return db_and()->condition($sql, $a[0], '>=')->condition($sql, $b[1], '<');
    }
    return NULL;
  }

  $values = array($parsed['a'], $parsed['b']);
  if ($kind == 'number') {
    foreach ($values as $i => $value) {
      if ($value === NULL) {
        continue;
      }
      // Both "1.5" and "1,5" are accepted.
      $normalized = (strpos($value, '.') === FALSE) ? str_replace(',', '.', $value) : str_replace(',', '', $value);
      if (!is_numeric($normalized)) {
        return NULL;
      }
      $values[$i] = $normalized + 0;
    }
  }
  switch ($op) {
    case '=':
      return db_and()->condition($sql, $values[0]);

    case '!':
      return db_or()->condition($sql, '%' . db_like($values[0]) . '%', 'NOT LIKE')->isNull($sql);

    case '!=':
      $or = db_or()->condition($sql, $values[0], '<>');
      if ($kind == 'text') {
        $or->isNull($sql);
      }
      return $or;

    case 'between':
      return db_and()->condition($sql, $values[0], '>=')->condition($sql, $values[1], '<=');

    case '<=':
    case '>':
      if ($kind == 'text') {
        // A text stands for everything that starts with it (like "2025"
        // stands for the whole year): "<=B" includes "Banana", ">B" does not.
        $like = db_like($values[0]) . '%';
        if ($op == '<=') {
          return db_or()->condition($sql, $values[0], '<=')->condition($sql, $like, 'LIKE');
        }
        return db_and()->condition($sql, $values[0], '>')->condition($sql, $like, 'NOT LIKE');
      }

    default:
      return db_and()->condition($sql, $values[0], $op);
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
      $global[] = array($sql, $view->field[$id]);
    }
    // Column filters come from the style options, never from the request.
    $mode = isset($filters[$id]) ? $filters[$id] : 'none';
    if ($col['search'] !== '' && in_array($mode, array('text', 'select')) && $id !== $exclude) {
      $column_conditions[] = array($sql, $col['search'], $mode, datatables_column_kind($view->field[$id]), datatables_date_column_info($view->field[$id]), $view->field[$id]);
    }
  }
  $tokens = ($request['search'] === '') ? array() : preg_split('/\s+/', $request['search']);

  foreach ($queries as $query) {
    // Global search: every word must match at least one searchable column.
    if ($role != 'count' && $global) {
      foreach ($tokens as $token) {
        $or = db_or();
        foreach ($global as $column) {
          $or->condition(datatables_server_contains_condition($column[1], $column[0], $token));
        }
        $query->condition($or);
      }
    }
    if ($role != 'count') {
      foreach ($column_conditions as $c) {
        if ($c[2] == 'select') {
          $query->condition($c[0], $c[1]);
          continue;
        }
        // Text filter: an operator at the start (">2025", "=abc", "1 - 5")
        // compares, anything else means "contains".
        $parsed = datatables_filter_parse($c[1]);
        if ($parsed && $parsed['op'] == 'between' && $c[3] == 'text') {
          // Ranges are for dates and numbers; text with a dash is searched.
          $parsed = NULL;
        }
        if ($parsed && $c[3] != 'none') {
          $condition = datatables_server_operator_condition($c[0], $parsed, $c[3], $c[4]);
          if ($condition) {
            $query->condition($condition);
          }
        }
        elseif (!$parsed) {
          $query->condition(datatables_server_contains_condition($c[5], $c[0], $c[1]));
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
 * Finds the display labels for the stored values of a field.
 *
 * Done for the fields whose stored value is not what the table shows:
 *  - list fields (text, integer, float, boolean): the label of the allowed
 *    value,
 *  - entity reference and taxonomy term reference fields: the label of the
 *    referenced entity (only entities the current user may view; for others
 *    the stored id stays).
 * Modules can still change any label with hook_datatables_filter_options_alter().
 *
 * @param views_handler $handler
 *   The field handler.
 * @param array $values
 *   The stored values.
 *
 * @return array
 *   Labels keyed by stored value; values without a label are left out.
 */
function datatables_server_value_labels($handler, array $values) {
  $labels = array();
  if ($handler instanceof views_handler_field_node_type) {
    // The content type column shows the name, unless it is set to show the
    // machine name.
    if (empty($handler->options['machine_name'])) {
      foreach ($values as $value) {
        $labels[$value] = (string) t(node_type_get_name($value));
      }
    }
    return $labels;
  }
  if ($handler instanceof views_handler_field_boolean) {
    // "Published" / "Not published", "Yes" / "No", ...: as the field is set
    // up to show the values.
    foreach ($values as $value) {
      $labels[$value] = datatables_server_boolean_label($handler, $value);
    }
    return $labels;
  }
  if (!($handler instanceof views_handler_field_field) || empty($handler->definition['field_name']) || !$values) {
    return $labels;
  }
  $field = field_info_field($handler->definition['field_name']);
  if (!$field) {
    return $labels;
  }

  if (in_array($field['type'], array('list_text', 'list_integer', 'list_float', 'list_boolean')) && function_exists('list_allowed_values')) {
    $allowed = list_allowed_values($field);
    foreach ($values as $value) {
      if (isset($allowed[$value]) && is_scalar($allowed[$value])) {
        $labels[$value] = (string) $allowed[$value];
      }
    }
  }
  elseif (in_array($field['type'], array('entityreference', 'taxonomy_term_reference'))) {
    $entity_type = ($field['type'] == 'entityreference') ? (isset($field['settings']['target_type']) ? $field['settings']['target_type'] : NULL) : 'taxonomy_term';
    if ($entity_type && entity_get_info($entity_type)) {
      $ids = array_values(array_filter($values, 'is_numeric'));
      $entities = $ids ? entity_load_multiple($entity_type, $ids) : array();
      foreach ($entities as $id => $entity) {
        if (entity_access('view', $entity_type, $entity)) {
          $label = entity_label($entity_type, $entity);
          if ($label !== NULL && $label !== '') {
            $labels[$id] = (string) $label;
          }
        }
      }
    }
  }
  return $labels;
}

/**
 * The text that a boolean field shows for a stored value.
 */
function datatables_server_boolean_label($handler, $value) {
  $row = new stdClass();
  $row->{$handler->field_alias} = $value;
  $label = trim(preg_replace('/\s+/', ' ', decode_entities(strip_tags((string) $handler->render($row)))));
  return ($label === '') ? (string) $value : $label;
}

/**
 * Finds the stored values of a column whose label contains a text.
 *
 * Some columns store a key or an id and show a label: the content type, list
 * fields, taxonomy terms and entity references. The table shows (and the
 * search in the browser looks at) the label, so the search in SQL has to look
 * for the stored values that have this label.
 *
 * @param object $handler
 *   The field handler.
 * @param string $term
 *   The text to find in the label (case insensitive).
 *
 * @return array|null
 *   The stored values (can be empty: nothing has such a label), or NULL if the
 *   column shows what is stored and can be searched directly.
 */
function datatables_server_label_values($handler, $term) {
  static $cache = array();
  $key = get_class($handler) . ':' . $handler->table . ':' . $handler->field . ':' . (isset($handler->options['id']) ? $handler->options['id'] : '') . ':' . backdrop_strtolower($term);
  if (!array_key_exists($key, $cache)) {
    $cache[$key] = _datatables_server_label_values($handler, $term);
  }
  return $cache[$key];
}

function _datatables_server_label_values($handler, $term) {
  $needle = backdrop_strtolower($term);
  $matches = function ($label) use ($needle) {
    $plain = trim(preg_replace('/\s+/', ' ', decode_entities(strip_tags((string) $label))));
    return strpos(backdrop_strtolower($plain), $needle) !== FALSE;
  };

  if ($handler instanceof views_handler_field_node_type) {
    if (!empty($handler->options['machine_name'])) {
      return NULL;
    }
    $found = array();
    foreach (node_type_get_names() as $type => $name) {
      if ($matches(t($name))) {
        $found[] = $type;
      }
    }
    return $found;
  }

  if ($handler instanceof views_handler_field_boolean) {
    $found = array();
    foreach (array(0, 1) as $value) {
      if ($matches(datatables_server_boolean_label($handler, $value))) {
        $found[] = (string) $value;
      }
    }
    return $found;
  }

  if (!($handler instanceof views_handler_field_field) || empty($handler->definition['field_name'])) {
    return NULL;
  }
  $field = field_info_field($handler->definition['field_name']);
  if (!$field) {
    return NULL;
  }

  if (in_array($field['type'], array('list_text', 'list_integer', 'list_float', 'list_boolean')) && function_exists('list_allowed_values')) {
    $found = array();
    foreach (list_allowed_values($field) as $value => $label) {
      if (is_scalar($label) && $matches($label)) {
        $found[] = (string) $value;
      }
    }
    return $found;
  }

  if (in_array($field['type'], array('entityreference', 'taxonomy_term_reference'))) {
    $entity_type = ($field['type'] == 'entityreference') ? (isset($field['settings']['target_type']) ? $field['settings']['target_type'] : NULL) : 'taxonomy_term';
    $info = $entity_type ? entity_get_info($entity_type) : NULL;
    if (!$info || empty($info['base table']) || empty($info['entity keys']['id']) || empty($info['entity keys']['label'])) {
      return NULL;
    }
    $query = db_select($info['base table'], 'e');
    $query->addField('e', $info['entity keys']['id'], 'id');
    $query->condition('e.' . $info['entity keys']['label'], '%' . db_like($term) . '%', 'LIKE');
    $query->range(0, 200);
    try {
      $ids = $query->execute()->fetchCol();
    }
    catch (Exception $e) {
      watchdog_exception('datatables', $e);
      return NULL;
    }
    // Only what the user may see: the same check as for the labels shown.
    $found = array();
    foreach ($ids ? entity_load_multiple($entity_type, $ids) : array() as $id => $entity) {
      if (entity_access('view', $entity_type, $entity)) {
        $found[] = (string) $id;
      }
    }
    return $found;
  }
  return NULL;
}

/**
 * The condition "the column contains a text", as the table shows it.
 *
 * @return object
 *   A condition object.
 */
function datatables_server_contains_condition($handler, $sql, $term) {
  $values = datatables_server_label_values($handler, $term);
  $condition = db_and();
  if ($values === NULL) {
    $condition->condition($sql, '%' . db_like($term) . '%', 'LIKE');
  }
  elseif ($values) {
    $condition->condition($sql, $values, 'IN');
  }
  else {
    // No label contains the text.
    $condition->where('1 = 0');
  }
  return $condition;
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
    // Fields that store a key or an id: show what the table shows.
    $field_labels = datatables_server_value_labels($handler, $values);
    if ($field_labels) {
      foreach ($field_labels as $value => $label) {
        $labels[$value] = truncate_utf8(trim(preg_replace('/\s+/', ' ', decode_entities(strip_tags($label)))), 100, TRUE, TRUE);
      }
      // The values were sorted by what is stored, not by what is shown.
      uasort($labels, function ($a, $b) {
        return strnatcasecmp($a, $b);
      });
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

/**
 * Starts the query log of a table request, if the style option "Debug" is on.
 *
 * Only users who may administer views get the queries.
 */
function datatables_server_debug_start($view) {
  $options = $view->display_handler->get_option('style_options');
  if (empty($options['elements']['debug_sql']) || !user_access('administer views')) {
    return;
  }
  Database::startLog('datatables', 'default');
  $view->datatables_debug = TRUE;
}

/**
 * Notes how many queries were logged so far (a phase of the request starts).
 */
function datatables_server_debug_mark($view, $phase) {
  if (empty($view->datatables_debug)) {
    return;
  }
  $logger = Database::getConnection('default')->getLogger();
  $view->datatables_debug_marks[$phase] = $logger ? count($logger->get('datatables')) : 0;
}

/**
 * Returns the logged SQL queries of the request for the browser console.
 *
 * The queries are grouped: the main query of the table (limited to the tables
 * of the view), the number of all records, and the lists of the column
 * filters. Everything done between them (rendering the fields, URL aliases and
 * so on) is left out. Each query has the values filled in ("sql") and the time
 * in milliseconds ("ms"). Returns NULL when the debug option is off or the
 * user may not see the queries.
 */
function datatables_server_debug_collect($view) {
  if (empty($view->datatables_debug) || !user_access('administer views')) {
    return NULL;
  }
  datatables_server_debug_mark($view, 'end');
  $marks = $view->datatables_debug_marks + array('render' => 0, 'total' => 0, 'filters' => 0, 'end' => 0);

  // The tables of the view: the base table, relationships and fields.
  $tables = array();
  if (!empty($view->query->table_queue)) {
    foreach ($view->query->table_queue as $info) {
      if (!empty($info['table']) && is_string($info['table'])) {
        $tables[$info['table']] = TRUE;
      }
    }
  }

  $log = Database::getConnection('default')->getLogger()->get('datatables');
  $phases = array(
    'main' => array('title' => 'Main query', 'from' => 0, 'to' => $marks['render'], 'restrict' => TRUE),
    'total' => array('title' => 'Number of all records', 'from' => $marks['total'], 'to' => $marks['filters'], 'restrict' => FALSE),
    'filters' => array('title' => 'Column filter lists', 'from' => $marks['filters'], 'to' => $marks['end'], 'restrict' => FALSE),
  );
  $groups = array();
  $total = 0;
  foreach ($phases as $phase) {
    $queries = array();
    $ms_group = 0;
    for ($i = $phase['from']; $i < $phase['to'] && isset($log[$i]); $i++) {
      $entry = $log[$i];
      // Housekeeping is not what the user looks for.
      if (preg_match('/information_schema|\burl_alias\b|\b(cache\w*|semaphore|sessions|watchdog|flood)\b/i', $entry['query'])) {
        continue;
      }
      if ($phase['restrict']) {
        // The lookups of labels for the search (list values, entity names) run
        // on other tables, and are marked by their alias "e".
        $used = strpos($entry['query'], 'SELECT `e`.') === 0;
        foreach ($tables as $table => $unused) {
          if (strpos($entry['query'], '`' . $table . '`') !== FALSE) {
            $used = TRUE;
            break;
          }
        }
        if (!$used) {
          continue;
        }
      }
      $replace = array();
      $args = is_array($entry['args']) ? $entry['args'] : array();
      foreach ($args as $name => $value) {
        if (is_null($value)) {
          $replace[$name] = 'NULL';
        }
        elseif (is_int($value) || is_float($value)) {
          $replace[$name] = (string) $value;
        }
        else {
          $replace[$name] = "'" . str_replace("'", "\\'", (string) $value) . "'";
        }
      }
      // strtr() replaces the longest names first, so :db_condition_placeholder_1
      // is not hit by :db_condition_placeholder_10.
      $sql = $replace ? strtr($entry['query'], $replace) : $entry['query'];
      $ms = round($entry['time'] * 1000, 2);
      $ms_group += $ms;
      $queries[] = array('sql' => trim($sql), 'ms' => $ms);
    }
    if ($queries) {
      $total += $ms_group;
      $groups[] = array('title' => $phase['title'], 'ms' => round($ms_group, 2), 'queries' => $queries);
    }
  }
  unset($view->datatables_debug);
  return array('groups' => $groups, 'ms' => round($total, 2));
}

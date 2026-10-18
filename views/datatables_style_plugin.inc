<?php
/**
 * @file
 * Contains the datatables style plugin.
 */

/**
 * Style plugin to render each item as a row in a datatables.
 *
 * @ingroup views_style_plugins
 */
class datatables_style_plugin extends views_plugin_style_table {
  /**
   * Implements parent::option_definition().
   */
  function option_definition() {
    $options = parent::option_definition();


    $options['elements'] = array(
      'default' => array(
        'search_box' => TRUE,
        'table_info' => TRUE,
        'save_state' => FALSE,
        'table_tools' => FALSE,
        'export_all' => TRUE,
        'export_limit' => 10000,
      ),
    );
    $options['layout'] = array(
      'default' => array(
        'autowidth' => FALSE,
        'themeroller' => FALSE,
        'sdom' => '',
      ),
    );
    $options['pages'] = array(
      'default' => array(
        'pagination_style' => 0,
        'vheight' => 300,
        'length_change' => 0,
        'display_length' => 10,
        'preview_limit' => 100,
      ),
    );
    $options['hidden_columns'] = array(
      'default' => array(),
    );
    // Per column filter: 'none', 'text' or 'select'.
    $options['filters'] = array(
      'default' => array(),
    );
    $options['sticky_offset'] = array('default' => '');
    $options['filter_options'] = array(
      'default' => array(
        'limit' => 100,
        'cascade' => TRUE,
      ),
    );
    return $options;
  }

  /**
   * Implements parent::options_form().
   */
  function options_form(&$form, &$form_state) {
    parent::options_form($form, $form_state);

    // Check if fields have been added.  Table style plugin will set
    // error_markup if fields are not added.
    // @see views_plugin_style_table::options_form()
    if (isset($form['error_markup'])) {
      return;
    }

    // The sticky header is done by this module (CSS), not by Backdrop's
    // tableheader.js.
    $form['sticky']['#title'] = t('Sticky table header');
    $form['sticky']['#description'] = t('Keep the column titles (and column filters) visible at the top of the window while scrolling the page. Not available together with a fixed table height (Scroller), whose header never scrolls away. Not active in the Views preview.');
    $form['sticky_offset'] = array(
      '#type' => 'textfield',
      '#title' => t('Fixed elements above the sticky header'),
      '#description' => t('Optional CSS selector of other elements that are fixed at the top of the window and must not be covered, e.g. <code>#site-banner, .navbar-fixed</code>. Fixed or sticky &lt;header&gt; elements and the administration bar are found automatically.'),
      '#default_value' => isset($this->options['sticky_offset']) ? $this->options['sticky_offset'] : '',
      '#states' => array(
        'visible' => array(
          ':input[name="style_options[sticky]"]' => array('checked' => TRUE),
        ),
      ),
    );
    // "Show the empty text in the table" makes no sense here: DataTables
    // shows its own "No matching records found" row.
    unset($form['empty_table']);
    unset($form['override']);

    $form['description_markup']['#markup'] = '<div class="description form-item">' . t('DataTables works best if you set the pager to display all items, since DataTabels contains its own pager implementation.<br/><br/>Place fields into columns; you may combine multiple fields into the same column. If you do, the separator in the column specified will be used to separate the fields. Check the sortable box to make that column click sortable, and check the default sort radio to determine which column will be sorted by default, if any. You may control column order and field labels in the fields section.') . '</div>';


    $form['elements'] = array(
      '#type' => 'fieldset',
      '#title' => t('Widgets & Elements'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    $form['elements']['search_box'] = array(
      '#type' => 'checkbox',
      '#title' => t('Enable the search filter box.'),
      '#default_value' => !empty($this->options['elements']['search_box']),
      '#description' => 'The search filter box allows users to dynamically filter the results in the table.  Disabling this option hides the search filter box; the column filters keep working.',
    );
    $form['elements']['table_info'] = array(
      '#type' => 'checkbox',
      '#title' => t('Enable the table information display.'),
      '#default_value' => !empty($this->options['elements']['table_info']),
      '#description' => t('This shows information about the data that is currently visible on the page, including information about filtered data if that action is being performed.'),
    );
    $form['elements']['save_state'] = array(
      '#type' => 'checkbox',
      '#title' => t('Save State'),
      '#default_value' => !empty($this->options['elements']['save_state']),
      '#description' => t("DataTables can use HTML5 localStorage and sessionStorage APIs in the end user's web-browser in order to store it's state after each change in drawing. What this means is that if the user were to reload the page, the table should remain exactly as it was (length, filtering, pagination and sorting)"),
    );
    $form['elements']['table_tools'] = array(
      '#type' => 'checkbox',
      '#title' => t('Buttons'),
      '#default_value' => !empty($this->options['elements']['table_tools']),
      '#description' => t('Buttons (successor of Table Tools) is a plugin that adds a powerful button toolbar with copy, save, and print capabilities. Note that if you have custom dom parameter set, Buttons can be included by inserting the "B" character.'),
    );

    $form['elements']['export_all'] = array(
      '#type' => 'checkbox',
      '#title' => t('Export all rows (server-side processing only)'),
      '#default_value' => !isset($this->options['elements']['export_all']) || !empty($this->options['elements']['export_all']),
      '#description' => t('With server-side processing the table only holds the rows of the current page. When this is enabled, the Export buttons (copy, CSV, Excel, PDF, print) request all rows that match the current search, column filters and sort order from the server. Otherwise only the visible page is exported.'),
      '#states' => array(
        'visible' => array(
          ':input[name="style_options[elements][table_tools]"]' => array('checked' => TRUE),
        ),
      ),
    );
    $form['elements']['export_limit'] = array(
      '#type' => 'number',
      '#title' => t('Maximum number of rows to export'),
      '#default_value' => isset($this->options['elements']['export_limit']) ? $this->options['elements']['export_limit'] : 10000,
      '#min' => 1,
      '#description' => t('Protects the server and the browser: large exports need a lot of memory and time, PDF files in particular. If more rows match, only the first ones are exported and the user is told. This limit is enforced on the server.'),
      '#element_validate' => array('element_validate_integer_positive'),
      '#states' => array(
        'visible' => array(
          ':input[name="style_options[elements][table_tools]"]' => array('checked' => TRUE),
          ':input[name="style_options[elements][export_all]"]' => array('checked' => TRUE),
        ),
      ),
    );

    $form['layout'] = array(
      '#type' => 'fieldset',
      '#title' => t('Layout and Display'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );

    $form['layout']['themeroller'] = array(
      '#type' => 'checkbox',
      '#title' => t('Enable jQuery UI ThemeRoller Support (Deprecated)'),
      '#default_value' => !empty($this->options['layout']['themeroller']),
      '#description' => t("Create markup and classes to support jQuery UI's ThemeRoller"),
    );

    $form['layout']['sdom'] = array(
      '#type' => 'textfield',
      '#title' => t('Set dom Initialization Parameter (Deprecated)'),
      '#default_value' => $this->options['layout']['sdom'],
      '#description' => t("Use the dom parameter to rearrange the interface elements. See the <a href='@sdom_documentation_url'>Datatables dom documentation</a> for details on how to use this feature", array('@sdom_documentation_url' => 'https://datatables.net/reference/option/dom')),
    );

    $form['pages'] = array(
      '#type' => 'fieldset',
      '#title' => t('Pagination and Page Length'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#description' => t('If this display uses AJAX (Advanced settings: <em>Use AJAX</em>), the rows of each page are requested from the server (<em>server-side processing</em>, suited for large data sets). Otherwise all rows are loaded at once and DataTables works in the browser. With <em>No Pagination</em> the Views pager is used and all rows of the current Views page are loaded.'),
    );
    $form['pages']['pagination_style'] = array(
      '#type' => 'select',
      '#title' => t('Pagination Style'),
      '#default_value' => isset($this->options['pages']['pagination_style']) ? $this->options['pages']['pagination_style'] : 0,
      '#options' => array(
        0 => t('Full Numbers (Default)'),
        'simple_numbers' => t('Simple Numbers'),
        'scroller' => t('Scroller'),
        'no_pagination' => t('No Pagination (use Views Pager)'),
      ),
      '#description' => t('Selects which pagination style should be used.'),
    );

    $form['pages']['vheight'] = array(
      '#type' => 'textfield',
      '#title' => t('Vertical height'),
      '#description' => t('Vertical height as any CSS unit, or a number in pixels.'),
      '#size'=> 15,
      '#default_value' => isset($this->options['pages']['vheight']) ? $this->options['pages']['vheight'] : '300',
      '#states' => array(
        'visible' => array(
          ':input[name="style_options[pages][pagination_style]"]' => array('value' => 'scroller'),
        ),
        'required' => array(
          ':input[name="style_options[pages][pagination_style]"]' => array('value' => 'scroller'),
        ),
      ),
    );

    $form['pages']['length_change'] = array(
      '#type' => 'checkbox',
      '#title' => t('Enable Length Change Selection Box'),
      '#description' => t('Enable page length selection menu.'),
      '#default_value' => !empty($this->options['pages']['length_change']),
      '#states' => array(
        'visible' => array(
          ':input[name="style_options[pages][pagination_style]"]' => array(
            array('value' => 0),
            array('value' => 'simple_numbers'),
          ),
        ),
      ),
    );

    $form['pages']['display_length'] = array(
      '#type' => 'textfield',
      '#title' => t('Default Page Length'),
      '#description' => t('Default number of records to show per page. May be adjusted by users if Length Selection is enabled.'),
      '#size'=> 10,
      '#default_value' => isset($this->options['pages']['display_length']) ? $this->options['pages']['display_length'] : 10,
      '#element_validate' => array('element_validate_integer_positive'),
      '#states' => array(
        'visible' => array(
          ':input[name="style_options[pages][pagination_style]"]' => array(
            array('value' => 0),
            array('value' => 'simple_numbers'),
          ),
        ),
      ),
    );

    $form['pages']['preview_limit'] = array(
      '#type' => 'number',
      '#title' => t('Rows in the Views preview'),
      '#description' => t('The preview of the Views editor is reloaded after every change. To keep it fast it only loads this many rows (0 = all). It does not affect the real page.'),
      '#size' => 10,
      '#min' => 0,
      '#default_value' => isset($this->options['pages']['preview_limit']) ? $this->options['pages']['preview_limit'] : 100,
    );

    $columns = $this->display->handler->get_field_labels();
    $form['hidden_columns'] = array(
      '#type' => 'fieldset',
      '#title' => t('Hidden and Expandable Columns'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );

    foreach ($columns as $column_name => $column_label) {
      $form['hidden_columns'][$column_name] = array(
        '#type' => 'select',
        '#title' => check_plain($column_label),
        '#default_value' => isset($this->options['hidden_columns'][$column_name]) ? $this->options['hidden_columns'][$column_name] : 0,
        '#options' => array(
          0 => 'Visible',
          'hidden' => 'Hidden',
          'expandable' => 'Hidden and Expandable',
        ),
      );
    }

    // Column filters.
    $form['filters'] = array(
      '#type' => 'fieldset',
      '#title' => t('Column filters'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
      '#description' => t('Adds a filter below the header of a column. A <em>drop-down</em> lists the distinct values of the column; with server-side processing (AJAX) they are read from the database, limited to the rows that are left by the other filters of the table (like Views Selective Filters). With server-side processing, columns that cannot be filtered in SQL (aggregated fields, formulas, global fields) get no filter.'),
    );
    foreach ($columns as $column_name => $column_label) {
      $form['filters'][$column_name] = array(
        '#type' => 'select',
        '#title' => check_plain($column_label),
        '#default_value' => isset($this->options['filters'][$column_name]) ? $this->options['filters'][$column_name] : 'none',
        '#options' => array(
          'none' => t('No filter'),
          'text' => t('Text box'),
          'select' => t('Drop-down with distinct values'),
        ),
      );
    }
    $form['filter_options'] = array(
      '#type' => 'fieldset',
      '#title' => t('Drop-down filter options'),
      '#collapsible' => TRUE,
      '#collapsed' => TRUE,
    );
    $form['filter_options']['limit'] = array(
      '#type' => 'number',
      '#title' => t('Maximum number of values'),
      '#description' => t('If a column has more distinct values than this, a text box is shown instead of the drop-down.'),
      '#default_value' => isset($this->options['filter_options']['limit']) ? $this->options['filter_options']['limit'] : 100,
      '#min' => 1,
      '#max' => 1000,
    );
    $form['filter_options']['cascade'] = array(
      '#type' => 'checkbox',
      '#title' => t('Only list values that are still possible'),
      '#description' => t('Server-side processing only. Recalculate the lists after every change, taking into account the other filters and the search box.'),
      '#default_value' => !empty($this->options['filter_options']['cascade']),
    );
  }

  /**
   * Tells whether DataTables fetches the rows from the server.
   *
   * This is derived from the Views settings: the display must use AJAX and
   * DataTables (not the Views pager) must do the paging. The Views live
   * preview has no AJAX settings, so it always works client-side.
   */
  function is_serverside() {
    if (!empty($this->view->live_preview)) {
      return FALSE;
    }
    if (!isset($this->options['pages']['pagination_style']) || $this->options['pages']['pagination_style'] === 'no_pagination') {
      return FALSE;
    }
    return !empty($this->view->display_handler) && $this->view->display_handler->use_ajax();
  }

  /**
   * Tells whether this request is a DataTables server-side draw.
   */
  protected function is_datatables_request() {
    return !empty($this->view->datatables_request) && $this->view->datatables_role == 'main';
  }

  /**
   * Overrides parent::even_empty().
   *
   * The table of a server-side DataTable is rendered without rows; DataTables
   * shows its own messages.
   */
  function even_empty() {
    // Not parent::even_empty(): that one honors the removed table option
    // "Show the empty text in the table".
    return $this->is_serverside();
  }

  /**
   * Overrides parent::init().
   */
  function init(&$view, &$display, $options = NULL) {
    parent::init($view, $display, $options);
    // The option "Show the empty text in the table" was removed. Views saved
    // with it enabled must not use it any more (it breaks searches without
    // result).
    $this->options['empty_table'] = FALSE;
  }

  /**
   * Returns the sort order requested by DataTables as a list of
   * array('field' => field id, 'dir' => 'asc'|'desc') for sortable columns.
   */
  protected function datatables_order() {
    $order = array();
    if (empty($this->view->datatables_request['order'])) {
      return $order;
    }
    foreach ($this->view->datatables_request['order'] as $item) {
      $id = $item['field'];
      if (!empty($this->view->field[$id])
        && !empty($this->options['info'][$id]['sortable'])
        && $this->view->field[$id]->click_sortable()) {
        $order[] = $item;
      }
    }
    return $order;
  }

  /**
   * Overrides parent::build_sort().
   *
   * A sort requested by DataTables replaces the sorts of the View, exactly
   * like a click on a column header does in a normal table.
   */
  function build_sort() {
    if ($this->datatables_order()) {
      return FALSE;
    }
    return parent::build_sort();
  }

  /**
   * Overrides parent::build_sort_post().
   *
   * Uses the click_sort() of the field handlers (the Views way to sort a
   * table column), for as many columns as DataTables asks for.
   */
  function build_sort_post() {
    $order = $this->datatables_order();
    if (!$order) {
      parent::build_sort_post();
    }
    else {
      $this->active = $order[0]['field'];
      $this->order = $order[0]['dir'];
      foreach ($order as $item) {
        $this->view->field[$item['field']]->click_sort($item['dir']);
      }
    }
    $this->add_sort_tiebreaker();
  }

  /**
   * Makes the order of rows with equal sort values deterministic.
   *
   * Without it the database may return such rows in a different order for
   * every query, so a page, the next page and an export of all rows (which
   * uses another LIMIT) can disagree and rows may appear twice or not at all
   * while paging. The unique key of the base table is added as last sort.
   */
  protected function add_sort_tiebreaker() {
    $view = $this->view;
    if (!$this->is_datatables_request() || empty($view->query) || !empty($view->query->groupby) || empty($view->base_field)) {
      return;
    }
    $view->query->add_orderby($view->base_table, $view->base_field, 'ASC');
  }

  /**
   * Overrides parent::render().
   *
   * For a DataTables request the rows are not themed to HTML; they are turned
   * into the JSON answer that is sent back by
   * datatables_views_ajax_data_alter().
   */
  function render() {
    if (!$this->is_datatables_request()) {
      return parent::render();
    }

    module_load_include('inc', 'views', 'templates/views.theme');
    module_load_include('inc', 'datatables', 'datatables.server');
    $request = $this->view->datatables_request;

    // Use the same code as the table template, so fields that share a column
    // are combined and classes/excluded fields are handled identically.
    $vars = array(
      'view' => $this->view,
      'rows' => $this->view->result,
      'options' => $this->options,
      'title' => '',
    );
    template_preprocess_views_view_table($vars);

    $data = array();
    foreach ($vars['rows'] as $i => $row) {
      $item = array();
      foreach ($row as $column => $html) {
        $item[$column] = (string) $html;
      }
      if (!empty($vars['row_classes'][$i]) && is_array($vars['row_classes'][$i])) {
        $item['DT_RowClass'] = implode(' ', $vars['row_classes'][$i]);
      }
      $data[] = $item;
    }

    $filtered = (int) $this->view->total_rows;
    $response = array(
      'draw' => $request['draw'],
      'recordsTotal' => datatables_server_total_count($this->view, $request),
      'recordsFiltered' => $filtered,
      'data' => $data,
    );
    if ($request['options']) {
      $response['filterOptions'] = datatables_server_filter_options($this->view, $request);
    }
    $this->view->datatables_response = $response;

    return '';
  }
}

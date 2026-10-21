/**
 * @file
 * Initialises DataTables for the DataTables Views style (and theme_datatable).
 *
 * Server-side processing uses Backdrop's own Views Ajax callback (views/ajax):
 * the DataTables parameters are POSTed together with the identification of the
 * view (name, display, arguments, path, dom id) that Views already puts in
 * Backdrop.settings.views.ajaxViews for Ajax-enabled views, plus the current
 * exposed filter input. The PHP side answers with an Ajax command
 * ("datatablesResponse") holding the JSON that DataTables expects.
 *
 * Column filters ("text" or "select") live in a second header row. With
 * server-side processing the lists of the "select" filters are delivered by
 * the server together with each draw; with client-side processing they are
 * read from the table data.
 */
(function ($) {

  "use strict";

  /**
   * Finds the Views Ajax settings of the view that contains a table.
   */
  function getViewSettings($table) {
    var match = ($table.closest('[class*="view-dom-id-"]').attr('class') || '').match(/view-dom-id-(\S+)/);
    if (!match || !Backdrop.settings.views || !Backdrop.settings.views.ajaxViews) {
      return null;
    }
    return Backdrop.settings.views.ajaxViews['views_dom_id:' + match[1]] || null;
  }

  function getAjaxPath() {
    var path = Backdrop.settings.views.ajax_path;
    return $.isArray(path) ? path[0] : path;
  }

  function escapeRegex(text) {
    return text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  }

  function plainText(html) {
    return $.trim($('<div>').html(html).text()).replace(/\s+/g, ' ');
  }

  /**
   * Splits the cell of a multi-value field into its single values.
   *
   * @param {string} html
   *   The cell content.
   * @param {Object} split
   *   How the field joins its values: {type: 'ul'|'ol'|'separator',
   *   separator: '...'} (from the field's "Display all values" settings).
   */
  function splitItems(html, split) {
    var parts = null;
    html = String(html);
    if (split.type === 'ul' || split.type === 'ol') {
      var $items = $('<div>').html(html).find('li');
      if ($items.length) {
        parts = $items.map(function () { return $(this).html(); }).get();
      }
    }
    else if (split.separator) {
      // Write the separator the way the browser wrote the cell (e.g. <br />
      // becomes <br>), so both can be compared.
      var markup = $('<div>').html(split.separator).html();
      if (markup !== '' && html.indexOf(markup) !== -1) {
        parts = html.split(markup);
      }
      else {
        // The browser may have written the separator differently (entities).
        var separator = plainText(split.separator);
        if (separator !== '' && plainText(html).indexOf(separator) !== -1) {
          parts = plainText(html).split(separator);
        }
      }
    }
    var items = [];
    $.each(parts || [html], function (i, part) {
      var text = plainText(part);
      if (text !== '' && $.inArray(text, items) < 0) {
        items.push(text);
      }
    });
    return items;
  }

  /**
   * Keeps the table header visible while the page scrolls.
   *
   * The real header cells are made "position: sticky" by css/datatables.css;
   * this function only provides the offsets: the height of fixed elements at
   * the top of the window (such as the administration bar, which announces
   * itself with a data-offset-top attribute, like for Backdrop's own sticky
   * headers), fixed <header> elements of the theme, and the height of the first header row, so that the filter row
   * sticks right below the titles.
   */
  function stickyHeader($table, extraSelector) {
    var table = $table[0];
    $table.addClass('datatables-sticky');

    // Sticky cells need a background that covers the rows scrolling below.
    var background = '';
    $table.add($table.parents()).each(function () {
      var color = $(this).css('background-color');
      if (color && color !== 'transparent' && !/rgba\(.*,\s*0\)$/.test(color)) {
        background = color;
        return false;
      }
    });
    table.style.setProperty('--datatables-sticky-bg', background || '#fff');

    // Bottom edge of fixed (or sticky) bars at the top of the window: header
    // elements of the theme and any elements matching the style option.
    function fixedBarsBottom() {
      var bottom = 0;
      var selector = 'header' + (extraSelector ? ', ' + extraSelector : '');
      var candidates;
      try {
        candidates = $(selector);
      }
      catch (e) {
        candidates = $('header');
      }
      candidates.each(function () {
        if ($.contains(table, this) || $(this).closest(table).length) {
          return;
        }
        var position = $(this).css('position');
        if (position !== 'fixed' && position !== 'sticky') {
          return;
        }
        var rect = this.getBoundingClientRect();
        // Only bars anchored to the top edge that are really visible.
        if (rect.height > 0 && rect.bottom > 0 && rect.top < window.innerHeight / 2 && $(this).css('visibility') !== 'hidden') {
          bottom = Math.max(bottom, Math.ceil(rect.bottom));
        }
      });
      return bottom;
    }

    function update() {
      var top = 0;
      $('[data-offset-top]').not('.sticky-header').each(function () {
        var value = parseInt(this.getAttribute('data-offset-top'), 10);
        if (!isNaN(value)) {
          top += value;
        }
      });
      top = Math.max(top, fixedBarsBottom());
      var $first = $table.children('thead').children('tr').first();
      table.style.setProperty('--datatables-sticky-top', top + 'px');
      table.style.setProperty('--datatables-sticky-row', $first.outerHeight() + 'px');
    }

    // Fixed bars may shrink or hide while scrolling.
    var pending = false;
    function schedule() {
      if (!pending) {
        pending = true;
        window.requestAnimationFrame(function () {
          pending = false;
          update();
        });
      }
    }

    update();
    $(window).on('resize.datatablesSticky offsettopchange.datatablesSticky scroll.datatablesSticky', schedule);
    return update;
  }

  /**
   * Replaces "js:Some.function.name" strings in the options by the function.
   *
   * PHP cannot pass functions to JavaScript; the options of a table (e.g. the
   * "customize" callback of a button) can name a function instead, see
   * hook_datatables_export_buttons_alter(). Only functions that exist as
   * global names are used.
   */
  function resolveCallbacks(value) {
    if (typeof value === 'string' && value.indexOf('js:') === 0) {
      var target = window;
      $.each(value.substr(3).split('.'), function (i, part) {
        target = target ? target[part] : undefined;
      });
      if (typeof target !== 'function') {
        if (window.console) {
          window.console.warn('DataTables: function ' + value.substr(3) + ' not found.');
        }
        return undefined;
      }
      return target;
    }
    if ($.isArray(value) || $.isPlainObject(value)) {
      $.each(value, function (key, item) {
        value[key] = resolveCallbacks(item);
      });
    }
    return value;
  }

  /**
   * Keeps the filter row out of exports (copy, CSV, Excel, PDF, print).
   *
   * The Buttons extension exports every header row of the table; the second
   * one only holds the column filters. Button definitions are changed so that
   * only the first header row is exported.
   */
  function exportTitleRowOnly(buttons) {
    return $.map(buttons, function (button) {
      if (typeof button === 'string') {
        button = {extend: button};
      }
      if (!button || typeof button !== 'object') {
        return [button];
      }
      if ($.isArray(button.buttons)) {
        button.buttons = exportTitleRowOnly(button.buttons);
      }
      if ($.inArray(button.extend, ['copy', 'copyHtml5', 'csv', 'csvHtml5', 'excel', 'excelHtml5', 'pdf', 'pdfHtml5', 'print']) !== -1) {
        var exportOptions = $.extend({}, button.exportOptions);
        var customize = exportOptions.customizeData;
        exportOptions.customizeData = function (data) {
          if (data.headerStructure && data.headerStructure.length > 1) {
            data.headerStructure = data.headerStructure.slice(0, 1);
          }
          if (customize) {
            customize.apply(this, arguments);
          }
        };
        button.exportOptions = exportOptions;
      }
      return [button];
    });
  }

  /**
   * Makes the export buttons export all rows (server-side processing).
   *
   * In server-side mode the table only holds the current page, which is all
   * the Buttons extension can export. The action of each export button is
   * wrapped: it loads all rows matching the current search, column filters
   * and sort order (the server limits their number, see the "Maximum number
   * of rows to export" style option), runs the original action on them
   * without drawing them, and then reloads the page that was shown before.
   *
   * @param {Array} buttons
   *   The Buttons definitions.
   * @param {Object} state
   *   Shared state; createAjax() reads state.exporting.
   */
  function exportAllRows(buttons, state) {
    var names = {copy: 'copyHtml5', csv: 'csvHtml5', excel: 'excelHtml5', pdf: 'pdfHtml5', print: 'print'};
    return $.map(buttons, function (button) {
      if (typeof button === 'string') {
        button = {extend: button};
      }
      if (!button || typeof button !== 'object') {
        return [button];
      }
      if ($.isArray(button.buttons)) {
        button.buttons = exportAllRows(button.buttons, state);
      }
      var type = names[button.extend] || button.extend;
      if ($.inArray(type, ['copyHtml5', 'csvHtml5', 'excelHtml5', 'pdfHtml5', 'print']) === -1) {
        return [button];
      }
      var original = button.action || ($.fn.dataTable.ext.buttons[type] || {}).action;
      if (typeof original !== 'function') {
        return [button];
      }
      button.action = function (e, dt, node, config, cb) {
        var self = this;
        if (state.exporting) {
          return;
        }
        state.exporting = true;
        state.exportResult = null;
        // The draw that follows the response holds all rows: export them
        // there, cancel the draw, and load the previous page again. (The
        // preDraw event also fires before the request is sent; that one is
        // ignored, createAjax() sets state.exportResult when the rows are in.)
        dt.on('preDraw.datatablesExport', function () {
          if (!state.exportResult) {
            return;
          }
          dt.off('preDraw.datatablesExport');
          var ok = state.exportResult === 'ok';
          state.exporting = false;
          state.exportResult = null;
          if (ok) {
            var info = dt.page.info();
            if (info.recordsDisplay > dt.rows().count()) {
              dt.buttons.info(
                Backdrop.t('Export'),
                Backdrop.t('The table has @total rows. Only the first @count rows are exported.', {'@total': info.recordsDisplay, '@count': dt.rows().count()}),
                5000
              );
            }
            original.call(self, e, dt, node, config, cb);
          }
          setTimeout(function () {
            dt.ajax.reload(null, false);
          }, 0);
          return false;
        });
        dt.ajax.reload(null, false);
      };
      return [button];
    });
  }

  /**
   * Makes a Scroller table use the height of the window down to its bottom.
   *
   * CSS alone cannot know how much room the elements above and below the
   * table need, so the height is measured: the space between the top of the
   * table body and the bottom of the window, minus the parts of the DataTables
   * layout below the body (info line, ...) and the margin given in the style
   * options. It is calculated again when the window or the layout changes.
   */
  function fillHeight(table, margin) {
    var settings = table.settings()[0];
    var container = $(table.table().container());
    var pending = false;

    function apply() {
      pending = false;
      var $body = container.find('div.dt-scroll-body').first();
      if (!$body.length) {
        return;
      }
      // Everything of the table that follows the scrolling body (info line,
      // paging, spacing).
      var below = Math.max(0, container[0].getBoundingClientRect().bottom - $body[0].getBoundingClientRect().bottom);
      // Position in the document, independent of the scroll position.
      var top = $body[0].getBoundingClientRect().top + window.pageYOffset;
      var height = Math.max(120, Math.floor(window.innerHeight - top - below - margin));
      if (settings.oScroll.sY === height + 'px') {
        return;
      }
      settings.oScroll.sY = height + 'px';
      $body.css({height: height + 'px', maxHeight: height + 'px'});
      if (table.scroller) {
        table.scroller.measure();
      }
    }

    function schedule() {
      if (!pending) {
        pending = true;
        window.requestAnimationFrame(apply);
      }
    }

    apply();
    $(window).on('resize.datatablesFill', schedule);
    table.on('draw column-visibility responsive-resize', schedule);
  }

  /**
   * Keeps the address bar in sync with the state of the table.
   *
   * Used when the display has "AJAX history" enabled (like Backdrop does for
   * the exposed filters, pager and sort links of an Ajax View). The search,
   * column filters, sort order, page and page length of the table are written
   * to the query string, and read back when the page is loaded, so a copied
   * URL shows the same table. The parameters are:
   *  - dt_q: global search,
   *  - dt_s: sort order, e.g. "title:asc,type:desc" (field ids),
   *  - dt_c: the visible columns (field ids), chosen with the Columns button,
   *  - dt_p: page number (1 = first page),
   *  - dt_l: page length,
   *  - dt_f[field_id]: column filter value.
   * Parameters are only present when they differ from the defaults. The
   * address bar is replaced, no history entries are added.
   */
  var historyRestored = {};

  /**
   * The default sort of the options as "column:dir,..." text; DataTables
   * accepts several notations for the "order" option.
   */
  function orderText(order, options) {
    if (!order) {
      return '';
    }
    if ($.isPlainObject(order)) {
      order = [order];
    }
    var columns = options.columns || [];
    return $.map(order, function (o) {
      var idx = $.isArray(o) ? o[0] : (o ? o.idx : undefined);
      var dir = $.isArray(o) ? o[1] : (o && o.dir) || 'asc';
      return columns[idx] && typeof columns[idx].data === 'string' ? columns[idx].data + ':' + dir : null;
    }).join(',');
  }

  /**
   * The ids of the columns that are visible by default, as comma list.
   */
  function visibleText(options) {
    return $.map(options.columns || [], function (column) {
      return column && typeof column.data === 'string' && column.visible !== false ? column.data : null;
    }).join(',');
  }

  function historyEnabled($table, cfg) {
    var view = getViewSettings($table);
    return !!(cfg && cfg.serverSide && view && view.enable_ajax_history && window.history && window.history.replaceState && window.URLSearchParams);
  }

  function fieldColumns(options) {
    var map = {};
    $.each(options.columns || [], function (i, column) {
      if (column && typeof column.data === 'string') {
        map[column.data] = i;
      }
    });
    return map;
  }

  function historyParamsOf(table, options, fields) {
    var ids = [];
    $.each(options.columns || [], function (i, column) {
      ids[i] = column && typeof column.data === 'string' ? column.data : null;
    });
    var params = {};
    var search = table.search();
    if (search) {
      params.dt_q = search;
    }
    params.dt_s = $.map(table.order(), function (o) {
      return ids[o[0]] ? ids[o[0]] + ':' + o[1] : null;
    }).join(',');
    var info = table.page.info();
    if (info.page > 0) {
      params.dt_p = String(info.page + 1);
    }
    params.dt_l = String(info.length);
    params.columns = [];
    params.filters = {};
    table.columns().every(function () {
      if (ids[this.index()] && this.visible()) {
        params.columns.push(ids[this.index()]);
      }
      var value = this.search();
      if (value && ids[this.index()]) {
        params.filters[ids[this.index()]] = value;
      }
    });
    return params;
  }

  /**
   * Reads the table state from the URL into the DataTables options.
   *
   * @return {Object}
   *   The defaults (order, page length) the options had before.
   */
  function restoreFromHistory(options) {
    var fields = fieldColumns(options);
    var defaults = {
      order: orderText(options.order, options),
      length: options.pageLength || 10,
      columns: visibleText(options)
    };
    var query = new URLSearchParams(window.location.search);
    var found = false;

    if (query.has('dt_q')) {
      options.search = {search: query.get('dt_q')};
      found = true;
    }
    if (query.has('dt_s')) {
      var order = [];
      $.each(query.get('dt_s').split(','), function (i, item) {
        var parts = item.split(':');
        if (fields.hasOwnProperty(parts[0]) && (parts[1] === 'asc' || parts[1] === 'desc')) {
          order.push([fields[parts[0]], parts[1]]);
        }
      });
      options.order = order;
      found = true;
    }
    var length = parseInt(query.get('dt_l'), 10);
    var menu = options.lengthMenu || [];
    var allowed = $.map(menu, function (m) { return $.isPlainObject(m) ? m.value : m; });
    if (length > 0 && $.inArray(length, allowed) !== -1) {
      options.pageLength = length;
      found = true;
    }
    var page = parseInt(query.get('dt_p'), 10);
    if (page > 1) {
      options.displayStart = (page - 1) * (options.pageLength || 10);
      found = true;
    }
    if (query.has('dt_c')) {
      // The columns that were visible; every other column is hidden.
      var shown = query.get('dt_c').split(',');
      var count = 0;
      $.each(options.columns || [], function (i, column) {
        if (column && typeof column.data === 'string' && $.inArray(column.data, shown) !== -1) {
          count++;
        }
      });
      if (count > 0) {
        $.each(options.columns || [], function (i, column) {
          if (column && typeof column.data === 'string') {
            column.visible = $.inArray(column.data, shown) !== -1;
          }
        });
        found = true;
      }
    }
    var columns = options.columns || [];
    query.forEach(function (value, key) {
      var match = key.match(/^dt_f\[(.+)\]$/);
      if (match && fields.hasOwnProperty(match[1])) {
        options.searchCols = options.searchCols || [];
        for (var i = 0; i < columns.length; i++) {
          options.searchCols[i] = options.searchCols[i] || null;
        }
        options.searchCols[fields[match[1]]] = {search: value};
        found = true;
      }
    });
    if (found) {
      // The URL is the state that was asked for.
      options.stateSave = false;
    }
    return defaults;
  }

  function writeHistory(table, options, defaults) {
    var params = historyParamsOf(table, options);
    var url = new URL(window.location.href);
    var query = url.searchParams;
    var remove = [];
    query.forEach(function (value, key) {
      if (/^dt_(q|s|p|l|c)$/.test(key) || /^dt_f\[/.test(key)) {
        remove.push(key);
      }
    });
    $.each(remove, function (i, key) { query.delete(key); });
    if (params.dt_q) {
      query.set('dt_q', params.dt_q);
    }
    if (params.dt_s !== defaults.order) {
      query.set('dt_s', params.dt_s);
    }
    if (params.dt_p) {
      query.set('dt_p', params.dt_p);
    }
    if (params.columns.join(',') !== defaults.columns) {
      query.set('dt_c', params.columns.join(','));
    }
    // With Scroller the page length is derived from the height of the table.
    if (!options.scroller && parseInt(params.dt_l, 10) !== parseInt(defaults.length, 10)) {
      query.set('dt_l', params.dt_l);
    }
    $.each(params.filters, function (id, value) {
      query.set('dt_f[' + id + ']', value);
    });
    var target = url.pathname + (query.toString() ? '?' + query.toString() : '') + url.hash;
    if (target !== window.location.pathname + window.location.search + window.location.hash) {
      window.history.replaceState(window.history.state, document.title, target);
    }
  }

  /**
   * Creates the DataTables "ajax" function for server-side processing.
   */
  function createAjax($table, cfg, state) {
    return function (data, callback) {
      var exporting = state.exporting && !state.exportResult;
      var fail = function (message) {
        if (exporting) {
          state.exportResult = 'failed';
        }
        callback({draw: data.draw, data: [], recordsTotal: 0, recordsFiltered: 0, error: message});
      };

      var view = getViewSettings($table);
      if (!view) {
        fail(Backdrop.t('DataTables server-side processing requires the view display to use AJAX.'));
        return;
      }

      var request = {
        dom_id: view.view_dom_id,
        draw: data.draw,
        start: data.start,
        length: data.length,
        search: {value: data.search.value},
        order: $.map(data.order || [], function (o) {
          return {column: o.column, dir: o.dir};
        }),
        columns: $.map(data.columns, function (c) {
          return {data: c.data, searchable: c.searchable, orderable: c.orderable, search: {value: c.search.value}};
        })
      };

      if (exporting) {
        // Export of all rows: the server decides how many (style option).
        request.export = 1;
        request.start = 0;
        request.length = -1;
      }
      // Ask for the lists of the drop-down filters when they are not known
      // yet, or on every draw when they depend on the other filters.
      else if (state.selects.length && (cfg.cascade || !state.optionsLoaded)) {
        request.options = state.selects;
      }

      // Same parameters as Views' own Ajax requests, plus the exposed input.
      var payload = $.extend({}, cfg.exposedInput || {}, {
        view_name: view.view_name,
        view_display_id: view.view_display_id,
        view_args: view.view_args,
        view_path: view.view_path,
        view_dom_id: view.view_dom_id,
        datatables: request
      });

      $.ajax({
        url: getAjaxPath(),
        type: 'POST',
        dataType: 'json',
        data: payload
      }).done(function (commands) {
        var response = null;
        $.each(commands || [], function (i, command) {
          if (command.command === 'datatablesResponse') {
            response = command.data;
            return false;
          }
        });
        if (!response) {
          fail(Backdrop.t('The server did not return any data. Check that the view display is accessible and does not use caching.'));
          return;
        }
        if (response.filterOptions) {
          state.optionsLoaded = true;
          if (state.setOptions) {
            state.setOptions(response.filterOptions);
          }
        }
        if (exporting) {
          state.exportResult = 'ok';
        }
        callback(response);
      }).fail(function (xhr) {
        fail(Backdrop.t('An error occurred while loading the table (HTTP @status).', {'@status': xhr.status}));
      });
    };
  }

  /**
   * Builds the column filter controls in the second header row.
   */
  function attachFilters(table, $table, cfg, state) {
    var controls = {};
    // Client-side drop-downs of multi-value fields: id => {column, split,
    // value, cache}. These are filtered by a custom search function that
    // looks at the single values of a cell instead of the whole cell text.
    var multi = {};

    function search(column, value, type, id) {
      if (multi[id]) {
        multi[id].value = value;
        table.draw();
      }
      else if (!cfg.serverSide && type === 'select') {
        // Client-side: exact match on the cell text.
        column.search(value === '' ? '' : '^' + escapeRegex(value) + '$', true, false).draw();
      }
      else {
        column.search(value).draw();
      }
    }

    function makeText(id, column, $cell) {
      var timer;
      var $input = $('<input type="text" class="form-text form-control datatables-filter datatables-filter-text">')
        .attr('placeholder', Backdrop.t('Filter'))
        .attr('aria-label', Backdrop.t('Filter'))
        .val(column.search())
        .on('click', function (e) { e.stopPropagation(); })
        .on('keyup input', function () {
          var value = this.value;
          clearTimeout(timer);
          timer = setTimeout(function () {
            if (column.search() !== value) {
              search(column, value, 'text');
            }
          }, 300);
        });
      $cell.empty().append($input);
      controls[id] = {type: 'text', column: column, $cell: $cell, $control: $input};
    }

    function makeSelect(id, column, $cell) {
      var $select = $('<select class="form-select datatables-filter datatables-filter-select"></select>')
        .attr('aria-label', Backdrop.t('Filter'))
        .on('click', function (e) { e.stopPropagation(); })
        .on('change', function () {
          search(column, $(this).val(), 'select', id);
        });
      $cell.empty().append($select);
      controls[id] = {type: 'select', column: column, $cell: $cell, $control: $select};
      fillSelect(id, []);
    }

    function fillSelect(id, values) {
      var control = controls[id];
      var current = multi[id] ? multi[id].value : control.column.search();
      if (!cfg.serverSide && !multi[id] && current) {
        // Client-side searches are stored as regular expressions.
        current = current.replace(/^\^|\$$/g, '').replace(/\\(.)/g, '$1');
      }
      var $select = control.$control.empty();
      $select.append($('<option>').val('').text(Backdrop.t('- All -')));
      var found = current === '';
      $.each(values, function (i, item) {
        $select.append($('<option>').val(item.value).text(item.label));
        if (item.value === current) {
          found = true;
        }
      });
      if (!found) {
        $select.append($('<option>').val(current).text(current));
      }
      $select.val(current);
    }

    // Create the controls.
    table.columns().every(function () {
      var column = this;
      var id = column.dataSrc();
      var type = cfg.filters[id];
      if (!type) {
        return;
      }
      // With a scrolling body DataTables keeps a hidden clone of the header
      // for sizing; only the real header row (table.table().header()) is used.
      var $cell = $(table.table().header()).find('tr.datatables-filters th[data-field="' + id + '"]');
      if (!$cell.length) {
        return;
      }
      if (type === 'select') {
        makeSelect(id, column, $cell);
        if (!cfg.serverSide) {
          var seen = {};
          var values = [];
          var split = cfg.split && cfg.split[id] ? cfg.split[id] : null;
          if (split) {
            multi[id] = {column: column, split: split, value: '', cache: {}};
          }
          $.each(column.data().toArray(), function (i, html) {
            // A multi-value cell contributes one option for every value.
            var texts = split ? splitItems(html, split) : [plainText(html)];
            $.each(texts, function (j, text) {
              if (text !== '' && !seen[text]) {
                seen[text] = true;
                values.push({value: text, label: text});
              }
            });
          });
          values.sort(function (a, b) {
            return a.label.localeCompare(b.label, undefined, {numeric: true});
          });
          fillSelect(id, values);
        }
      }
      else {
        makeText(id, column, $cell);
      }
    });

    if (!$.isEmptyObject(multi)) {
      $.fn.dataTable.ext.search.push(function (settings, data, dataIndex) {
        if (settings.nTable !== table.table().node()) {
          return true;
        }
        var ok = true;
        $.each(multi, function (id, info) {
          if (info.value === '') {
            return;
          }
          var items = info.cache[dataIndex];
          if (!items) {
            items = info.cache[dataIndex] = splitItems(table.cell(dataIndex, info.column.index()).data(), info.split);
          }
          if ($.inArray(info.value, items) < 0) {
            ok = false;
            return false;
          }
        });
        return ok;
      });
    }

    // Server-side: lists arrive with the draws.
    state.setOptions = function (lists) {
      $.each(lists, function (id, info) {
        var control = controls[id];
        if (!control || control.type !== 'select') {
          return;
        }
        if (info.truncated) {
          // Too many values for a drop-down: use a text box instead.
          control.$control.remove();
          makeText(id, control.column, control.$cell);
          state.selects = $.grep(state.selects, function (s) { return s !== id; });
          return;
        }
        fillSelect(id, info.values);
      });
    };
  }

  Backdrop.behaviors.datatables = {
    attach: function (context, settings) {
      $.each(settings.datatables, function (selector) {
        $(selector, context).once('datatables', function () {
          var $table = $(this);
          // Work on a copy: Backdrop.settings may be merged again later.
          var options = $.extend(true, {}, settings.datatables[selector]);
          var cfg = options.backdrop || null;
          delete options.backdrop;
          resolveCallbacks(options);

          options.responsive = true;

          // With a filter row, DataTables 2.0 would write the column titles
          // into the bottom header row. The labels are already in the first
          // row of the table markup, so let DataTables read them from there.
          if (cfg && !$.isEmptyObject(cfg.filters) && options.columns) {
            $.each(options.columns, function (i, column) {
              delete column.title;
            });
          }

          if (cfg && !$.isEmptyObject(cfg.filters) && $.isArray(options.buttons)) {
            options.buttons = exportTitleRowOnly(options.buttons);
          }

          var state = {selects: [], optionsLoaded: false, setOptions: null, exporting: false};
          if (cfg && cfg.serverSide && cfg.exportAll && $.isArray(options.buttons)) {
            options.buttons = exportAllRows(options.buttons, state);
          }
          if (cfg && cfg.serverSide) {
            // Known before the first request, which is sent by the constructor.
            $.each(cfg.filters || {}, function (id, type) {
              if (type === 'select') {
                state.selects.push(id);
              }
            });
          }
          if (cfg && cfg.serverSide) {
            options.ajax = createAjax($table, cfg, state);
          }

          // Table state in the URL (display option "AJAX history"). Only the
          // first load of a page reads it: when the View is reloaded after an
          // exposed filter changed, the table starts again from the defaults.
          var historyDefaults = null;
          if (historyEnabled($table, cfg)) {
            var historyKey = getViewSettings($table).view_name + ':' + getViewSettings($table).view_display_id;
            historyDefaults = historyRestored[historyKey] ? {
              order: orderText(options.order, options),
              length: options.pageLength || 10,
              columns: visibleText(options)
            } : restoreFromHistory(options);
            historyRestored[historyKey] = true;
          }

          var table = $table.DataTable(options);

          if (historyDefaults) {
            table.on('draw column-visibility', function () {
              writeHistory(table, options, historyDefaults);
            });
          }

          if (cfg && !$.isEmptyObject(cfg.filters)) {
            attachFilters(table, $table, cfg, state);
          }
          if (cfg && cfg.hideSearch) {
            $(table.table().container()).find('.dt-search').hide();
          }
          if (options.responsive && table.responsive) {
            // Measure again once fonts and scrollbars are in place.
            var remeasure = function () {
              table.columns.adjust();
              table.responsive.recalc();
            };
            if (document.readyState === 'complete') {
              setTimeout(remeasure, 0);
            }
            else {
              $(window).one('load', remeasure);
            }
          }
          if (cfg && cfg.fillHeight !== null && cfg.fillHeight !== undefined && table.scroller) {
            fillHeight(table, cfg.fillHeight);
          }
          if (cfg && cfg.sticky) {
            var updateSticky = stickyHeader($table, cfg.stickyOffset);
            // Header heights change with the filter controls and the layout.
            table.on('draw column-visibility responsive-resize', updateSticky);
          }
        });
      });
    }
  };

})(jQuery);

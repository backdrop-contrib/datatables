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
      if ($.inArray(button.extend, ['copy', 'csv', 'excel', 'pdf', 'print']) !== -1) {
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
   * Creates the DataTables "ajax" function for server-side processing.
   */
  function createAjax($table, cfg, state) {
    return function (data, callback) {
      var fail = function (message) {
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

      // Ask for the lists of the drop-down filters when they are not known
      // yet, or on every draw when they depend on the other filters.
      if (state.selects.length && (cfg.cascade || !state.optionsLoaded)) {
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

          var state = {selects: [], optionsLoaded: false, setOptions: null};
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

          var table = $table.DataTable(options);

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

// jQueryIdjQuery

Drupal.behaviors.datatables = {
  attach: function (context) {
    jQuery.each(Drupal.settings.datatables, function (selector) {
      // Check if table contains expandable hidden rows.
      if (this.bExpandable) {
        // Insert a "view more" column to the table.
        var nCloneTh = document.createElement('th');
        var nCloneTd = document.createElement('td');
        nCloneTd.innerHTML = '<a href="#" class="datatables-expand datatables-closed">Show Details</a>';

        jQuery(selector + ' thead tr').each( function () {
          this.insertBefore( nCloneTh, this.childNodes[0] );
        });

        jQuery(selector + ' tbody tr').each( function () {
          this.insertBefore(  nCloneTd.cloneNode( true ), this.childNodes[0] );
        });

        this.aoColumns.unshift({"bSortable": false});
      }

      // Sanity Check: before creating a datatable, make sure it is not already
      // created.  If it is, remove old version before new one is created.
      var table = jQuery(selector);
      if (table) {
        // Pull in dataTables master settings.
        var datatable_settings = jQuery(document).dataTableSettings;
        if (datatable_settings != 'undefined') {
          for (var i=0; i < datatable_settings.length; i++) {
            if (datatable_settings[i].sInstance = selector) {
              datatable_settings.splice(i, 1);
              break;
            }
          }
        }
      }

      var datatable = jQuery(selector).dataTable(this);

      if (this.bExpandable) {
        // Add column headers to table settings.
        var settings = datatable.fnSettings();
        // Add blank column header for show details column.
        this.aoColumnHeaders.unshift('');
        // Add column headers to table settings.
        settings.aoColumnHeaders = this.aoColumnHeaders;

        /* Add event listener for opening and closing details
        * Note that the indicator for showing which row is open is not controlled by DataTables,
        * rather it is done here
        */
        jQuery('td a.datatables-expand', datatable.fnGetNodes() ).each( function () {
          jQuery(this).click( function () {
            var row = this.parentNode.parentNode;
            if (jQuery(this).hasClass('datatables-open')) {
              datatable.fnClose(row);
              jQuery(this).removeClass('datatables-open').addClass('datatables-closed').html('Show Details');
            }
            else {
              datatable.fnOpen( row, Drupal.theme('datatablesExpandableRow', datatable, row), 'details' );
              jQuery(this).removeClass('datatables-closed').addClass('datatables-open').html('Hide Details');
            }
            return false;
          });
        });
      }
    });
  }
};

/**
 * Theme an expandable hidden row.
 *
 * @param object
 *   The datatable object.
 * @param array
 *   The row array for which the hidden row is being displayed.
 * @return
 *   The formatted text (html).
 */
Drupal.theme.prototype.datatablesExpandableRow = function(datatable, row) {
  var rowData = datatable.fnGetData(row);
  var settings = datatable.fnSettings();

  var output = '<table style="padding-left: 50px">';
  jQuery.each(rowData, function(index) {
    if (!settings.aoColumns[index].bVisible) {
      output += '<tr><td>' + settings.aoColumnHeaders[index] + '</td><td style="text-align: left">' + this + '</td></tr>';
    }
  });
  output += '</table>';
  return output;
};

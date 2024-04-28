(function ($) {
  Backdrop.behaviors.datatables = {
    attach: function (context, settings) {
      $.each(settings.datatables, function (selector) {
        $(selector, context).once('datatables', function() {
          // Check if table contains expandable hidden rows.
          var settings = Backdrop.settings.datatables[selector];
/*
          if (settings.expandable) {
            // Insert a "view more" column to the table.
            settings.responsive = { details: { type: 'column' } };
            settings.columns.unshift({title: '', data: false, defaultContent: '', className: 'dt-control', orderable: false, visible: true});
            settings.order.idx = settings.order.idx + 1;

            var nCloneTh = document.createElement('th');
            var nCloneTd = document.createElement('td');
            nCloneTd.innerHTML = '<a href="#" class="datatables-expand datatables-closed">' + Backdrop.t('Show Details') + '</a>';

            $(selector + ' thead tr').each( function () {
              this.insertBefore( nCloneTh, this.childNodes[0] );
            });

            $(selector + ' tbody tr').each( function () {
              this.insertBefore(  nCloneTd.cloneNode( true ), this.childNodes[0] );
            });
          }
*/
          settings.responsive = true;
/*          settings.responsive = { details: {
            display: DataTable.Responsive.display.modal({
                header: function (row) {
                    var data = row.data();
                    return 'Details for ' + data[0] + ' ' + data[1];
                }
            }),
            renderer: DataTable.Responsive.renderer.tableAll()
          } };
*/
          var table = $(selector).DataTable(settings);
        });
      });
    }
  };
})(jQuery);

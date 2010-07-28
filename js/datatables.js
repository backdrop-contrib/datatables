Drupal.behaviors.datatables = {
  attach: function (context) {
    console.debug(Drupal.settings.datatables);
    jQuery.each(Drupal.settings.datatables, function (selector) {
      jQuery(selector).dataTable(this);
    });
  }
};

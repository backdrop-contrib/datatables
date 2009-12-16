Drupal.behaviors.datatables = function (context) {
  $.each(Drupal.settings.datatables, function (selector) {
    $(selector).dataTable(this);
  });
}

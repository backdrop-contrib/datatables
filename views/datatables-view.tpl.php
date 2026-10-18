<?php
/**
 * @file
 * Template to display a datatable.
 *
 * - $title : The title of this group of rows.  May be empty.
 * - $header: An array of header labels keyed by field id.
 * - $fields: An array of CSS IDs to use for each field id.
 * - $classes: A class or classes to apply to the table, based on settings.
 * - $row_classes: An array of classes to apply to each row, indexed by row
 *   number. This matches the index in $rows.
 * - $rows: An array of row items. Each row is an array of content.
 *   $rows are keyed by row number, fields within rows are keyed by field ID.
 * - $data_order: Sort keys of date cells (row number, field id), may be empty.
 * - $filters: An array of column filter types ('text' or 'select'), keyed by
 *   field ID. When not empty, a second header row is added; js/datatables.js
 *   fills it with the filter controls.
 */
?>
<?php /* width="100%": DataTables only follows window resizes (and so lets Responsive hide columns) for tables with an explicit width. */ ?>
<table id="<?php print $id ?>" class="<?php print implode(' ', $classes); ?>" width="100%"<?php print backdrop_attributes($attributes); ?>>
  <?php if (!empty($title)) : ?>
    <caption><?php print $title; ?></caption>
  <?php endif; ?>
  <thead>
    <tr>
      <?php foreach ($header as $field => $label): ?>
        <th class="views-field views-field-<?php print $fields[$field]; ?>">
          <?php print $label; ?>
        </th>
      <?php endforeach; ?>
    </tr>
    <?php if (!empty($filters)): ?>
      <tr class="datatables-filters">
        <?php foreach ($header as $field => $label): ?>
          <th class="datatables-filter-cell<?php print isset($filters[$field]) ? ' datatables-filter-' . $filters[$field] : ''; ?>" data-field="<?php print $field; ?>"></th>
        <?php endforeach; ?>
      </tr>
    <?php endif; ?>
  </thead>
  <tbody>
    <?php foreach ($rows as $count => $row): ?>
      <tr class="<?php print implode(' ', $row_classes[$count]); ?>">
        <?php foreach ($row as $field => $content): ?>
          <td class="views-field views-field-<?php print $fields[$field]; ?>"<?php if (isset($data_order[$count][$field]) && $data_order[$count][$field] !== ''): ?> data-order="<?php print check_plain($data_order[$count][$field]); ?>"<?php endif; ?>>
            <?php print $content; ?>
          </td>
        <?php endforeach; ?>
      </tr>
    <?php endforeach; ?>
  </tbody>
</table>

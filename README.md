DataTables
==========

This module provides Views integration for the DataTables library, which 
provides advanced interaction controls to HTML tables such as dynamic
pagination, on-the-fly filtering, and column sorting.

In contrast to the Drupal module, which used DataTables v1.9 from 2012, the 
Backdrop module uses v2 from 2024.

Version 2.1 of this module introduces two useful features of DataTables:

- **Server-side processing** which draws each table page using an Ajax request to the server and therefore is useful to show large data sets.

- **Scroller** which draws only the visible portion of the table (and a bit to either side to make the scrolling smooth), while the scrolling container gives the visual impression that the whole table is visible.

Both features can be enabled and used independently but they naturally fit well together. See a DataTables example with both features at https://datatables.net/extensions/scroller/examples/initialisation/server-side_processing.html.

For full documentation and examples, visit the DataTables website: https://datatables.net/.

Installation
------------

- Install this module using the [official Backdrop CMS instructions]((https://docs.backdropcms.org/documentation/extend-with-modules).

- There are two ways you can use DataTables:
  
  1. Create a View and use the DataTables display format.
  2. Use theme_datatable function in your own module code. Detailed 
     documentation is included with the function.

Differences from Drupal 7
-------------------------

- The DataTables library version 2.0.5 is alreay packaged with this module and does not need to be installed separately.
- DataTables requires jQuery 1.7 or newer which is already included in Backdrop.
- The following DataTables extensions are also included:
  * Buttons 3.0.2 (successor of TableTools) 
  * Responsive 3.0.2

Issues
------

Bugs and feature requests should be reported in [the Issue Queue](https://github.com/backdrop-contrib/foo-project/issues).

Current Maintainers
-------------------

- [Alex Höbart](https://github.com/AlexHoebart-ICPDR).
- Seeking additional maintainers.

Credits
-------

- Ported to Backdrop CMS by [Alex Höbart](https://github.com/AlexHoebart-ICPDR).
- Originally written for Drupal by [Jon Duell](https://www.drupal.org/u/duellj),
  [dqd](https://www.drupal.org/u/dqd), [Mitch Portier (Arkener)](https://www.drupal.org/u/arkener).
- Based on DataTables written by [Allan Jardine/
  SpryMedia](https://sprymedia.co.uk/about).

License
-------

This project is GPL v2 software.
See the LICENSE.txt file in this directory for complete text.
The DataTables library is available under the [MIT license](https://www.datatables.net/license/mit).

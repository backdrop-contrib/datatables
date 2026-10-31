/**
 * @file
 * Gives window.exports back, see exports-guard-start.js.
 */
(function () {
  'use strict';
  if (window.datatablesExportsGuard) {
    try {
      // Removing the property uncovers the element again.
      delete window.exports;
    }
    catch (error) {
    }
    delete window.datatablesExportsGuard;
  }
})();

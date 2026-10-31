/**
 * @file
 * Protects the DataTables libraries from a page element named "exports".
 *
 * The libraries (UMD modules) decide whether they run in a CommonJS
 * environment by testing typeof exports. An element with id="exports" (or
 * name="exports") on the page becomes window.exports, the libraries then
 * call require() or module.exports and fail. For the time the libraries load,
 * window.exports is hidden; exports-guard-end.js restores it.
 */
(function () {
  'use strict';
  try {
    var e = window.exports;
    if (e && typeof e === 'object' && e.nodeType) {
      Object.defineProperty(window, 'exports', {value: undefined, writable: true, configurable: true});
      window.datatablesExportsGuard = true;
    }
  }
  catch (error) {
    // Nothing to protect, or not possible: the libraries load as before.
  }
})();

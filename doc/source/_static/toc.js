/* Read-the-Docs style collapsible sidebar table of contents.
 *
 * The Weasel docs render the full nested global toctree. This script makes
 * each branch collapsible via a caret, auto-expands the branch containing the
 * current page, and keeps everything else collapsed by default. */
document.addEventListener('DOMContentLoaded', function () {
  var sidebar = document.querySelector('.sphinxsidebar');
  if (!sidebar) {
    return;
  }

  var items = sidebar.querySelectorAll('li');
  items.forEach(function (li) {
    if (li.querySelector('ul')) {
      li.classList.add('has-children');
      var caret = document.createElement('span');
      caret.className = 'caret';
      li.insertBefore(caret, li.firstChild);
      caret.addEventListener('click', function (e) {
        e.preventDefault();
        e.stopPropagation();
        li.classList.toggle('open');
      });
    }
  });

  /* Auto-expand the branch that contains the current page. */
  var current = sidebar.querySelector('li.current');
  var node = current;
  while (node && node !== sidebar) {
    if (node.tagName === 'LI') {
      node.classList.add('open');
    }
    node = node.parentElement;
  }
  if (current) {
    current.classList.add('open');
  }
});

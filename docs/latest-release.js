/* Optional stable-version display; download buttons never depend on API/JavaScript. */
'use strict';
(function () {
  const els = document.querySelectorAll('[data-latest-stable-version]');
  if (!els.length || !window.fetch) return;
  fetch('https://api.github.com/repos/masarray/arzoom-follow-obs/releases/latest', { headers: { Accept: 'application/vnd.github+json' } })
    .then(function (response) { if (!response.ok) throw new Error('release unavailable'); return response.json(); })
    .then(function (release) {
      if (!release || release.draft || release.prerelease ||
          !/^v[0-9]+[.][0-9]+[.][0-9]+$/.test(release.tag_name)) return;
      els.forEach(function (el) { el.textContent = release.tag_name; });
    })
    .catch(function () { /* Keep the server-rendered version; download remains available. */ });
}());

/* Grotto Documentation — Sidebar Toggle, Code Copy, Active Highlights */

document.addEventListener('DOMContentLoaded', function () {

  // ── Sidebar toggle (mobile) ──────────────────────────
  var toggle = document.querySelector('.sidebar-toggle');
  var sidebar = document.querySelector('.sidebar');
  var overlay = document.querySelector('.sidebar-overlay');

  if (toggle && sidebar) {
    toggle.addEventListener('click', function () {
      sidebar.classList.toggle('open');
      if (overlay) overlay.classList.toggle('visible');
    });
  }

  if (overlay) {
    overlay.addEventListener('click', function () {
      sidebar.classList.remove('open');
      overlay.classList.remove('visible');
    });
  }

  // ── Code copy buttons ────────────────────────────────
  document.querySelectorAll('.code-copy').forEach(function (btn) {
    btn.addEventListener('click', function () {
      var block = btn.closest('.code-block');
      var code = block ? block.querySelector('code') : null;
      if (!code) return;

      var text = code.textContent;
      navigator.clipboard.writeText(text).then(function () {
        btn.textContent = 'Copied!';
        btn.classList.add('copied');
        setTimeout(function () {
          btn.textContent = 'Copy';
          btn.classList.remove('copied');
        }, 2000);
      });
    });
  });

  // ── Active page highlighting ─────────────────────────
  var currentPage = window.location.pathname.split('/').pop() || 'index.html';
  document.querySelectorAll('.sidebar-nav a').forEach(function (link) {
    var href = link.getAttribute('href');
    if (href === currentPage) {
      link.classList.add('active');
    }
  });

  // ── Smooth scroll for anchor links ───────────────────
  document.querySelectorAll('a[href^="#"]').forEach(function (link) {
    link.addEventListener('click', function (e) {
      var targetId = link.getAttribute('href').substring(1);
      var target = document.getElementById(targetId);
      if (target) {
        e.preventDefault();
        target.scrollIntoView({ behavior: 'smooth', block: 'start' });

        // Update TOC active state
        document.querySelectorAll('.sidebar-toc a').forEach(function (a) {
          a.classList.remove('active');
        });
        link.classList.add('active');

        // Close mobile sidebar
        if (sidebar) sidebar.classList.remove('open');
        if (overlay) overlay.classList.remove('visible');
      }
    });
  });

  // ── Scroll spy for TOC ───────────────────────────────
  var tocLinks = document.querySelectorAll('.sidebar-toc a');
  if (tocLinks.length > 0) {
    var sections = [];
    tocLinks.forEach(function (link) {
      var id = link.getAttribute('href').substring(1);
      var el = document.getElementById(id);
      if (el) sections.push({ el: el, link: link });
    });

    var onScroll = function () {
      var scrollPos = window.scrollY + 120;
      var current = null;

      for (var i = 0; i < sections.length; i++) {
        if (sections[i].el.offsetTop <= scrollPos) {
          current = sections[i];
        }
      }

      tocLinks.forEach(function (l) { l.classList.remove('active'); });
      if (current) current.link.classList.add('active');
    };

    window.addEventListener('scroll', onScroll, { passive: true });
    onScroll();
  }
});

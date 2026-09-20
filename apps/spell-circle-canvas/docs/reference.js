// The two behaviours the reference layer needs, and nothing else.
//
// The language tabs remember one choice for the whole site, because a
// reader who writes Python does not want to choose again on every page.
//
// The search is TYPED. A reader of this site asks "what accepts a
// Fill", "what are the verbs in the Paint group", "what is bound in
// Python" — questions a full-text index over rendered prose cannot
// answer and a field query over the generated index can.

(function () {
  "use strict";

  var CHOICE = "sigil-reference-language";

  // ------------------------------------------------------- language tabs

  function remembered() {
    try {
      return window.localStorage.getItem(CHOICE) || "cpp";
    } catch (error) {
      return "cpp";
    }
  }

  function remember(language) {
    try {
      window.localStorage.setItem(CHOICE, language);
    } catch (error) {
      // A private window refuses storage; the choice then lasts the page.
    }
  }

  function showLanguage(language) {
    document.querySelectorAll(".languages").forEach(function (widget) {
      var panes = widget.querySelectorAll("pre.pane");
      var wanted = widget.querySelector('pre.pane[data-language="' + language + '"]')
        ? language
        : panes.length
          ? panes[0].dataset.language
          : "";
      panes.forEach(function (pane) {
        pane.hidden = pane.dataset.language !== wanted;
      });
      widget.querySelectorAll("button.tab").forEach(function (tab) {
        tab.setAttribute("aria-selected", String(tab.dataset.language === wanted));
      });
    });
  }

  function wireTabs() {
    document.addEventListener("click", function (event) {
      var tab = event.target.closest("button.tab");
      if (!tab) return;
      remember(tab.dataset.language);
      showLanguage(tab.dataset.language);
    });
    showLanguage(remembered());
  }

  // -------------------------------------------------------------- search

  var rows = null;
  var loading = null;

  // The index is a script rather than a fetch, because a page opened
  // straight off the disk cannot read a file beside it. It is loaded on
  // the first search, so a reader who never searches never pays for it.
  function load() {
    if (loading) return loading;
    loading = new Promise(function (settle) {
      if (window.referenceIndex) {
        rows = window.referenceIndex;
        settle(rows);
        return;
      }
      var script = document.createElement("script");
      script.src = window.referenceRoot + "search-index.js";
      script.onload = function () {
        rows = window.referenceIndex || [];
        settle(rows);
      };
      script.onerror = function () {
        rows = [];
        settle(rows);
      };
      document.head.appendChild(script);
    });
    return loading;
  }

  // A query is a run of `field:value` terms and free words. Every term
  // has to match; the free words are matched against the name first and
  // the summary second, so an exact name sorts above a mention of it.
  function parse(query) {
    var fields = [];
    var words = [];
    query
      .trim()
      .split(/\s+/)
      .forEach(function (piece) {
        if (!piece) return;
        var split = piece.indexOf(":");
        if (split > 0) {
          fields.push([
            piece.slice(0, split).toLowerCase(),
            piece.slice(split + 1).toLowerCase()
          ]);
        } else {
          words.push(piece.toLowerCase());
        }
      });
    return { fields: fields, words: words };
  }

  function matchesField(row, field, value) {
    if (field === "accepts") {
      return (row.accepts || []).some(function (one) {
        return one.toLowerCase().indexOf(value) >= 0;
      });
    }
    if (field === "returns") return (row.returns || "").toLowerCase().indexOf(value) >= 0;
    if (field === "kind") return (row.kind || "").toLowerCase() === value;
    if (field === "library") return (row.library || "").toLowerCase().indexOf(value) >= 0;
    if (field === "group") return (row.group || "").toLowerCase().indexOf(value) >= 0;
    if (field === "python") {
      if (value === "yes") return Boolean(row.python);
      if (value === "no") return !row.python;
      return (row.python || "").toLowerCase().indexOf(value) >= 0;
    }
    return false;
  }

  function score(row, words) {
    if (!words.length) return 1;
    var name = (row.name || "").toLowerCase();
    var summary = (row.summary || "").toLowerCase();
    var python = (row.python || "").toLowerCase();
    var total = 0;
    for (var index = 0; index < words.length; index += 1) {
      var word = words[index];
      if (name === word) total += 100;
      else if (name.indexOf(word) === 0) total += 60;
      else if (name.indexOf(word) >= 0) total += 30;
      else if (python.indexOf(word) >= 0) total += 20;
      else if (summary.indexOf(word) >= 0) total += 5;
      else return 0;
    }
    return total;
  }

  function search(query) {
    var asked = parse(query);
    var found = [];
    for (var index = 0; index < rows.length; index += 1) {
      var row = rows[index];
      var keep = asked.fields.every(function (pair) {
        return matchesField(row, pair[0], pair[1]);
      });
      if (!keep) continue;
      var rank = score(row, asked.words);
      if (rank > 0) found.push([rank, row]);
    }
    found.sort(function (left, right) {
      return right[0] - left[0] || left[1].name.localeCompare(right[1].name);
    });
    return found.slice(0, 40).map(function (pair) {
      return pair[1];
    });
  }

  function draw(found, panel) {
    if (!found.length) {
      panel.innerHTML = '<p class="empty">Nothing matches.</p>';
      panel.hidden = false;
      return;
    }
    panel.innerHTML = found
      .map(function (row) {
        var where = [row.kind, row.library, row.group].filter(Boolean).join(" · ");
        return (
          '<a href="' +
          window.referenceRoot +
          row.url +
          '"><span class="kind">' +
          escape(where) +
          "</span><br><b>" +
          escape(row.name) +
          "</b>" +
          (row.python ? " <code>" + escape(row.python) + "</code>" : "") +
          '<div class="what">' +
          escape(row.summary || "") +
          "</div></a>"
        );
      })
      .join("");
    panel.hidden = false;
  }

  function escape(text) {
    return String(text).replace(/[&<>"]/g, function (character) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[character];
    });
  }

  function wireSearch() {
    var input = document.getElementById("find");
    var panel = document.getElementById("results");
    if (!input || !panel) return;
    var run = function () {
      var query = input.value.trim();
      if (!query) {
        panel.hidden = true;
        return;
      }
      load().then(function () {
        draw(search(query), panel);
      });
    };
    input.addEventListener("input", run);
    input.addEventListener("focus", load);
    document.addEventListener("click", function (event) {
      if (!panel.contains(event.target) && event.target !== input) panel.hidden = true;
    });
    document.addEventListener("keydown", function (event) {
      if (event.key === "Escape") panel.hidden = true;
      if (event.key === "/" && document.activeElement !== input) {
        event.preventDefault();
        input.focus();
      }
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    wireTabs();
    wireSearch();
  });
})();

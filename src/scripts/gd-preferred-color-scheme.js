(function () {
  if (window.__gdPcsInjected) return;
  window.__gdPcsInjected = true;

  console.log("[GoldenDict] gd-preferred-color-scheme injected @", location.href);

  // 1) Enforce a light root color-scheme so UA-provided chrome (canvas,
  //    scrollbars, form controls, system colors) follows GoldenDict-ng's
  //    light theme instead of an OS-level dark appearance.
  try {
    var style = document.getElementById("gd-preferred-color-scheme");
    if (!style) {
      style = document.createElement("style");
      style.id = "gd-preferred-color-scheme";
      (document.head || document.documentElement).appendChild(style);
    }
    style.textContent = ":root,html{color-scheme:light !important;}";
  } catch (e) {}

  // 2) Override window.matchMedia for prefers-color-scheme queries so scripts
  //    perceive a light scheme regardless of the OS appearance.
  try {
    if (typeof window.matchMedia === "function" && !window.__gdOrigMatchMedia) {
      window.__gdOrigMatchMedia = window.matchMedia.bind(window);
      window.matchMedia = function (query) {
        var q = String(query);
        if (q.indexOf("prefers-color-scheme") !== -1) {
          return {
            matches: q.indexOf("light") !== -1,
            media: q,
            onchange: null,
            addListener: function () {},
            removeListener: function () {},
            addEventListener: function () {},
            removeEventListener: function () {},
            dispatchEvent: function () {
              return false;
            },
          };
        }
        return window.__gdOrigMatchMedia(q);
      };
    }
  } catch (e) {}

  // 3) Strip CSS @media (prefers-color-scheme: dark) blocks. The Blink engine
  //    evaluates such media queries from the OS appearance, so they still
  //    activate under an OS dark theme even though GoldenDict-ng is in light
  //    mode. The matchMedia override above cannot reach CSS media queries, so
  //    remove those rules once the stylesheets are available.
  function gdNeutralizeDarkMedia() {
    try {
      var sheets = document.styleSheets;
      for (var si = 0; si < sheets.length; si++) {
        var rules;
        try {
          rules = sheets[si].cssRules || sheets[si].rules;
        } catch (e) {
          continue;
        } // cross-origin
        if (!rules) continue;
        for (var i = rules.length - 1; i >= 0; i--) {
          var r = rules[i];
          if (r && r.type === 4 /* CSSRule.MEDIA_RULE */) {
            var cond = r.conditionText || (r.media && r.media.mediaText) || "";
            if (
              cond.indexOf("prefers-color-scheme") !== -1 &&
              cond.indexOf("dark") !== -1
            ) {
              try {
                sheets[si].deleteRule(i);
              } catch (e) {}
            }
          }
        }
      }
    } catch (e) {}
  }
  function gdScheduleSheet(node) {
    if (node && node.tagName === "LINK" && !node.sheet) {
      node.addEventListener("load", gdNeutralizeDarkMedia);
      node.addEventListener("error", gdNeutralizeDarkMedia);
    } else {
      gdNeutralizeDarkMedia();
    }
  }
  function gdOnDomReady() {
    var links = document.querySelectorAll('link[rel~="stylesheet"]');
    for (var li = 0; li < links.length; li++) gdScheduleSheet(links[li]);
    gdNeutralizeDarkMedia();
  }
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", gdOnDomReady);
  } else {
    gdOnDomReady();
  }
  window.addEventListener("load", gdNeutralizeDarkMedia);
  try {
    new MutationObserver(function (ms) {
      for (var mi = 0; mi < ms.length; mi++) {
        var ns = ms[mi].addedNodes;
        for (var ni = 0; ni < ns.length; ni++) {
          var n = ns[ni];
          if (!n || !n.tagName) continue;
          if (
            n.tagName === "LINK" &&
            (n.rel || "").indexOf("stylesheet") !== -1
          )
            gdScheduleSheet(n);
          else if (n.tagName === "STYLE") gdNeutralizeDarkMedia();
        }
      }
    }).observe(document.documentElement || document, {
      childList: true,
      subtree: true,
    });
  } catch (e) {}
})();

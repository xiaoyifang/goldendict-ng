function gdMakeArticleActive(newId, noEvent) {
  const gdCurrentArticle =
    document.querySelector(".gdactivearticle").attributes.id;
  if (gdCurrentArticle !== "gdfrom-" + newId) {
    document
      .querySelector(".gdactivearticle")
      .classList.remove("gdactivearticle");
    const newFormId = "gdfrom-" + newId;
    document.querySelector(`#${newFormId}`).classList.add("gdactivearticle");
    if (!noEvent) articleview.onJsActiveArticleChanged("gdfrom-" + newId);
  }
}

var overIframeId = null;

function gdSelectArticle(id) {
  var selection = window.getSelection();
  var range = document.createRange();
  range.selectNodeContents(document.getElementById("gdfrom-" + id));
  selection.removeAllRanges();
  selection.addRange(range);
}

function processIframeMouseOut() {
  overIframeId = null;
  top.focus();
}

function processIframeMouseOver(newId) {
  overIframeId = newId;
}

function processIframeClick() {
  if (overIframeId != null) {
    overIframeId = overIframeId.replace("gdexpandframe-", "");
    gdMakeArticleActive(overIframeId);
  }
}

function init() {
  window.addEventListener("blur", processIframeClick, false);
}
window.addEventListener("load", init, false);

function gdExpandOptPart(expanderId, optionalId) {
  const d1 = document.getElementById(expanderId);
  const isExpanded = d1.alt === "[+]";

  d1.alt = isExpanded ? "[-]" : "[+]";
  d1.src = isExpanded
    ? "qrc:///icons/collapse_opt.svg"
    : "qrc:///icons/expand_opt.svg";

  document
    .getElementById(optionalId)
    ?.querySelectorAll(".dsl_opt")
    .forEach((d2) => {
      d2.style.display = isExpanded ? "inline" : "none";
    });
}

function emitClickedEvent(link) {
  try {
    if ("string" != typeof link) {
      return;
    }
    articleview.linkClickedInHtml(link);
  } catch (error) {
    console.error(error);
  }
}

function gdExpandArticle(id) {
  emitClickedEvent();
  elem = document.getElementById("gdarticlefrom-" + id);
  ico = document.getElementById("expandicon-" + id);
  art = document.getElementById("gdfrom-" + id);
  ev = window.event;
  t = null;
  if (ev) t = ev.target || ev.srcElement;
  if (elem.style.display == "inline") {
    elem.style.display = "none";
    ico.className = "gdexpandicon";
    art.className = art.className + " gdcollapsedarticle";
    nm = document.getElementById("gddictname-" + id);
    nm.style.cursor = "pointer";
    if (ev) ev.stopPropagation();
    nm.title = "";
    articleview.collapseInHtml(id, true);
  } else if (elem.style.display == "none") {
    elem.style.display = "inline";
    ico.className = "gdcollapseicon";
    art.className = art.className.replace(" gdcollapsedarticle", "");
    nm = document.getElementById("gddictname-" + id);
    nm.style.cursor = "default";
    nm.title = "";
    articleview.collapseInHtml(id, false);
  }
}

function gdCheckArticlesNumber() {
  elems = document.getElementsByClassName("gddictname");
  if (elems.length == 1) {
    el = elems.item(0);
    s = el.id.replace("gddictname-", "");
    el = document.getElementById("gdfrom-" + s);
    if (el && el.className.search("gdcollapsedarticle") > 0) gdExpandArticle(s);
  }
}


function attachEventHandlers() {
  // Select all div elements with the class gdarticle
  const gdArticles = document.querySelectorAll('.gdarticle');

  // Attach event listeners to each gdarticle div
  gdArticles.forEach(function(article) {
      article.addEventListener('click', handleArticleEvent);
      article.addEventListener('contextmenu', handleArticleEvent);
  });

  function handleArticleEvent(event) {
      // Get the _id attribute
      const articleId = event.target.getAttribute('_id');
      if (typeof gdMakeArticleActive !== 'undefined') {
          gdMakeArticleActive(articleId, false);
      }
      // Prevent default context menu for oncontextmenu
      if (event.type === 'contextmenu') {
          event.preventDefault();
      }
  }
}

// Check the document ready state
if (document.readyState === 'complete' || document.readyState === 'interactive') {
  attachEventHandlers();
} else {
  document.addEventListener('DOMContentLoaded', attachEventHandlers);
}
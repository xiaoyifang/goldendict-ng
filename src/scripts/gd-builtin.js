function gdMakeArticleActive(newId, noEvent) {
  // Find the current active article and get its id using optional chaining
  const gdCurrentArticleId =
    document.querySelector(".gdactivearticle")?.attributes.id?.value;

  // Check if the current active article id matches the new id
  if (gdCurrentArticleId !== "gdfrom-" + newId) {
    // Remove the "gdactivearticle" class from the current active article if it exists
    document
      .querySelector(".gdactivearticle")
      ?.classList.remove("gdactivearticle");

    // Find the new article by id
    const newFormId = "gdfrom-" + newId;
    const newArticle = document.querySelector(`#${newFormId}`);

    // Add the "gdactivearticle" class to the new article if it exists
    newArticle?.classList.add("gdactivearticle");

    // Trigger the event if noEvent is false and articleview.onJsActiveArticleChanged is defined
    if (
      !noEvent &&
      typeof articleview.onJsActiveArticleChanged !== "undefined"
    ) {
      articleview.onJsActiveArticleChanged(newFormId);
    }
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

function gdAttachEventHandlers() {
  // Select all div elements with the class gdarticle
  const gdArticles = document.querySelectorAll(".gdarticle");

  // Attach event listeners to each gdarticle div
  gdArticles.forEach(function (article) {
    article.addEventListener("click", gdHandleArticleEvent);
    article.addEventListener("contextmenu", gdHandleArticleEvent);
  });

  document.body.addEventListener("click", function (event) {
    // Use closest to find the nearest parent div with the class 'gddictname'
    const dictNameElement = event.target.closest(".gddictname");

    if (dictNameElement) {
      // Get the data-gd-id attribute from the parent div
      const articleId = dictNameElement
        .closest(".gdarticle")
        ?.getAttribute("data-gd-id");

      gdExpandArticle(articleId);
    }
  });

  function gdHandleArticleEvent(event) {
    // Get the _id attribute
    const articleId = event.target
      .closest(".gdarticle")
      ?.getAttribute("data-gd-id");
    gdMakeArticleActive(articleId, false);
  }
}

// Check the document ready state
if (
  document.readyState === "complete" ||
  document.readyState === "interactive"
) {
  gdAttachEventHandlers();
} else {
  document.addEventListener("DOMContentLoaded", gdAttachEventHandlers);
}

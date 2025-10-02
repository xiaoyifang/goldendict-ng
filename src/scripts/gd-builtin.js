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

  const articleContent = document.getElementById("gdarticlefrom-" + id);
  const expandIcon = document.getElementById("expandicon-" + id);
  const articleElement = document.getElementById("gdfrom-" + id);
  const dictNameElement = document.getElementById("gddictname-" + id);

  if (!articleContent || !expandIcon || !articleElement || !dictNameElement) {
    console.warn("One or more required elements not found for id:", id);
    return;
  }

  const isExpanded = articleContent.style.display === "inline";

  if (isExpanded) {
    articleContent.style.display = "none";
    expandIcon.className = "gdexpandicon";
    articleElement.classList.add("gdcollapsedarticle");

    dictNameElement.style.cursor = "pointer";
    dictNameElement.title = "";

    articleview.collapseInHtml(id, true);
  } else {
    articleContent.style.display = "inline";
    expandIcon.className = "gdcollapseicon";
    articleElement.classList.remove("gdcollapsedarticle");

    dictNameElement.style.cursor = "default";
    dictNameElement.title = "";

    articleview.collapseInHtml(id, false);
  }
}

function gdCheckArticlesNumber() {
  const dictNameElements = document.getElementsByClassName("gddictname");

  if (dictNameElements.length === 1) {
    const dictNameElement = dictNameElements[0];
    const articleId = dictNameElement.id.replace("gddictname-", "");
    const articleElement = document.getElementById("gdfrom-" + articleId);

    if (
      articleElement &&
      articleElement.className.includes("gdcollapsedarticle")
    ) {
      gdExpandArticle(articleId);
    }
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

      event.stopPropagation();
    }
  });

  function gdHandleArticleEvent(event) {
    // Get the _id attribute
    const articleId = event.target
      .closest(".gdarticle")
      ?.getAttribute("data-gd-id");
    gdMakeArticleActive(articleId, false);
  }

  handleIframeEvents();
}

function handleIframeEvents() {
  const iframes = document.querySelectorAll("iframe[data-gd-id]");

  iframes.forEach((iframe) => {
    const gdId = iframe.getAttribute("data-gd-id");

    iframe.addEventListener("mouseover", function () {
      processIframeMouseOver("gdexpandframe-" + gdId);
    });

    iframe.addEventListener("mouseout", function () {
      processIframeMouseOut();
    });
  });
}

// DOM attribute monitoring related methods
let attributeMonitors = [];

/**
 * Monitor specific attribute changes of a single DOM element
 * @param {HTMLElement} element - The DOM element to monitor
 * @param {string|string[]} attributes - The attribute names to monitor, can be a single attribute or array of attributes
 * @param {Function} callback - Callback function when attribute changes
 * @returns {MutationObserver} - Returns the created observer instance
 */
function gdMonitorElementAttributes(element, attributes, callback) {
  if (!element || !attributes) return null;
  
  // Convert single attribute to array format
  const attributeList = Array.isArray(attributes) ? attributes : [attributes];
  
  const observer = new MutationObserver((mutationsList) => {
    for (let mutation of mutationsList) {
      if (mutation.type === 'attributes' && attributeList.includes(mutation.attributeName)) {
        const oldValue = mutation.oldValue;
        const newValue = element.getAttribute(mutation.attributeName);
        console.log(`Attribute changed: ${mutation.attributeName} - from "${oldValue}" to "${newValue}"`);
        
        if (typeof callback === 'function') {
          callback(mutation.attributeName, oldValue, newValue, element);
        }
      }
    }
  });
  
  observer.observe(element, {
    attributes: true,
    attributeOldValue: true,
    attributeFilter: attributeList
  });
  
  attributeMonitors.push(observer);
  return observer;
}

/**
 * Monitor src attribute changes of all img elements
 * @param {Function} callback - Callback function when attribute changes
 * @returns {MutationObserver} - Returns the created observer instance
 */
function gdMonitorImageSources(callback) {
  return gdMonitorElementsBySelector('img', ['src'], callback);
}

/**
 * Monitor href attribute changes of all a elements
 * @param {Function} callback - Callback function when attribute changes
 * @returns {MutationObserver} - Returns the created observer instance
 */
function gdMonitorLinkHrefs(callback) {
  return gdMonitorElementsBySelector('a', ['href'], callback);
}

/**
 * Monitor attribute changes of multiple elements based on CSS selector
 * @param {string} selector - CSS selector
 * @param {string|string[]} attributes - The attribute names to monitor
 * @param {Function} callback - Callback function when attribute changes
 * @returns {MutationObserver} - Returns the created observer instance
 */
function gdMonitorElementsBySelector(selector, attributes, callback) {
  // Convert single attribute to array format
  const attributeList = Array.isArray(attributes) ? attributes : [attributes];
  
  // Monitor existing elements
  const elements = document.querySelectorAll(selector);
  const elementObservers = Array.from(elements).map(element => 
    gdMonitorElementAttributes(element, attributeList, callback)
  );
  
  // Monitor newly added elements
  const observer = new MutationObserver((mutationsList) => {
    for (let mutation of mutationsList) {
      if (mutation.type === 'childList') {
        // Check added nodes
        mutation.addedNodes.forEach(node => {
          if (node.nodeType === 1) { // Element node
            // Check if the node itself matches the selector
            if (node.matches(selector)) {
              gdMonitorElementAttributes(node, attributeList, callback);
            }
            
            // Check if child elements match the selector
            const childElements = node.querySelectorAll(selector);
            childElements.forEach(element => 
              gdMonitorElementAttributes(element, attributeList, callback)
            );
          }
        });
      }
    }
  });
  
  // Start monitoring child node changes in the document
  observer.observe(document.body, {
    childList: true,
    subtree: true
  });
  
  attributeMonitors.push(observer);
  return observer;
}

/**
 * Stop all active attribute monitoring
 */
function gdStopAllAttributeMonitoring() {
  attributeMonitors.forEach(observer => {
    try {
      observer.disconnect();
    } catch (error) {
      console.error("Error stopping monitoring:", error);
    }
  });
  attributeMonitors = [];
}

// Initialize attribute monitoring functionality
function gdInitAttributeMonitoring() {
  // Automatically monitor all image src attribute changes
  gdMonitorImageSources((attr, oldVal, newVal, element) => {
    // Default image src change handling logic
    console.log(`Image resource changed: ${element.src}`);
  });
  
  // Automatically monitor all link href attribute changes
  gdMonitorLinkHrefs((attr, oldVal, newVal, element) => {
    // Default link href change handling logic
    console.log(`Link address changed: ${element.href}`);
  });
}

// Check the document ready state
if (
  document.readyState === "complete" ||
  document.readyState === "interactive"
) {
  gdAttachEventHandlers();
  gdInitAttributeMonitoring();
} else {
  document.addEventListener("DOMContentLoaded", () => {
    gdAttachEventHandlers();
    gdInitAttributeMonitoring();
  });
}

/**
 * Usage examples:
 * 
 * 1. Monitor specific attributes of a single element:
 *    const element = document.getElementById('myElement');
 *    gdMonitorElementAttributes(element, 'src', (attr, oldVal, newVal, elem) => {
 *      console.log(`${elem.id}'s ${attr} attribute changed from ${oldVal} to ${newVal}`);
 *    });
 * 
 * 2. Monitor src attribute of all images:
 *    gdMonitorImageSources((attr, oldVal, newVal, element) => {
 *      console.log(`Image ${element.alt || 'unnamed'}'s src attribute changed`);
 *    });
 * 
 * 3. Monitor href attribute of all links:
 *    gdMonitorLinkHrefs((attr, oldVal, newVal, element) => {
 *      console.log(`Link ${element.textContent}'s href attribute changed`);
 *    });
 * 
 * 4. Monitor specific elements using CSS selector:
 *    gdMonitorElementsBySelector('.dynamic-content', ['style', 'class'], (attr, oldVal, newVal, element) => {
 *      console.log(`Dynamic content area's ${attr} attribute changed`);
 *    });
 * 
 * 5. Stop all monitoring:
 *    gdStopAllAttributeMonitoring();
 */

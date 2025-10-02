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
      if (
        mutation.type === "attributes" &&
        attributeList.includes(mutation.attributeName)
      ) {
        const oldValue = mutation.oldValue;
        const newValue = element.getAttribute(mutation.attributeName);
        console.log(
          `Attribute changed: ${mutation.attributeName} - from "${oldValue}" to "${newValue}"`,
        );

        if (typeof callback === "function") {
          callback(mutation.attributeName, oldValue, newValue, element);
        }
      }
    }
  });

  observer.observe(element, {
    attributes: true,
    attributeOldValue: true,
    attributeFilter: attributeList,
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
  return gdMonitorElementsBySelector("img", ["src"], callback);
}

/**
 * Monitor href attribute changes of all a elements
 * @param {Function} callback - Callback function when attribute changes
 * @returns {MutationObserver} - Returns the created observer instance
 */
function gdMonitorLinkHrefs(callback) {
  return gdMonitorElementsBySelector("a", ["href"], callback);
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
  const elementObservers = Array.from(elements).map((element) =>
    gdMonitorElementAttributes(element, attributeList, callback),
  );

  // Monitor newly added elements
  const observer = new MutationObserver((mutationsList) => {
    for (let mutation of mutationsList) {
      if (mutation.type === "childList") {
        // Check added nodes
        mutation.addedNodes.forEach((node) => {
          if (node.nodeType === 1) {
            // Element node
            // Check if the node itself matches the selector
            if (node.matches(selector)) {
              gdMonitorElementAttributes(node, attributeList, callback);
            }

            // Check if child elements match the selector
            const childElements = node.querySelectorAll(selector);
            childElements.forEach((element) =>
              gdMonitorElementAttributes(element, attributeList, callback),
            );
          }
        });
      }
    }
  });

  // Start monitoring child node changes in the document
  observer.observe(document.body, {
    childList: true,
    subtree: true,
  });

  attributeMonitors.push(observer);
  return observer;
}

/**
 * Stop all active attribute monitoring
 */
function gdStopAllAttributeMonitoring() {
  attributeMonitors.forEach((observer) => {
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
  // Automatically monitor all image src attribute changes - only for relative paths
  monitorOnlyRelativePaths("img", "src", (attr, oldVal, newVal, element) => {
    // Default image src change handling logic
    console.log(`Image resource changed: ${element.src}`);

    // Process relative links for images
    processRelativeLink(element, newVal);
  });

  // Automatically monitor all link href attribute changes - only for relative paths
  monitorOnlyRelativePaths("a", "href", (attr, oldVal, newVal, element) => {
    // Default link href change handling logic
    console.log(`Link address changed: ${element.href}`);

    // Process relative links for links and resource files
    processRelativeLink(element, newVal);
  });
}

// Helper function to check if URL is absolute
function isAbsoluteUrl(url) {
  return url && (url.includes("://") || url.startsWith("//"));
}

// Helper function to monitor only relative paths
function monitorOnlyRelativePaths(selector, attribute, callback) {
  // Create a filtered callback that only processes relative paths
  const filteredCallback = (attr, oldVal, newVal, element) => {
    // If the new value is an absolute URL, skip processing
    if (!isAbsoluteUrl(newVal)) {
      callback(attr, oldVal, newVal, element);
    }
  };

  // Monitor all elements but filter in the callback
  return gdMonitorElementsBySelector(selector, [attribute], filteredCallback);
}

// Regular expression for matching resource file extensions
const RESOURCE_FILE_REGEX =
  /\.(jpg|jpeg|png|gif|webp|svg|js|css|json|xml|woff|woff2|ttf|eot)$/i;

/**
 * Process relative links for images and resource files
 * @param {HTMLElement} element - The DOM element with the link
 * @param {string} url - The URL to process
 */
function processRelativeLink(element, url) {
  if (!url) return;

  // Store original URL in a data attribute for potential reference
  if (!element.dataset.originalUrl) {
    element.dataset.originalUrl = url;
  }

  // Check if it's a relative link
  const isRelative = !isAbsoluteUrl(url);

  if (isRelative) {
    // Check if it's an image or resource file (js, css, etc.)
    const isResourceFile = RESOURCE_FILE_REGEX.test(url);
    const isImageTag = element.tagName.toLowerCase() === "img";

    if (isResourceFile || isImageTag) {
      // Find the parent div.gdarticle element and get its data-gd-id
      const articleElement = element.closest(".gdarticle");
      const dictId = articleElement
        ? articleElement.getAttribute("data-gd-id")
        : null;

      // Sanitize dictId to be only alphanumeric, dash, underscore. If not, skip.
      if (dictId && /^[a-zA-Z0-9_-]+$/.test(dictId)) {
        // Create the bres URL format: bres://[dictId]/relativePath
        // Remove leading slashes from relative path to avoid double slashes
        const relativePath = url.replace(/^\//, "");
        const bresUrl = `bres://${dictId}/${relativePath}`;

        // Update the appropriate attribute based on the element type
        if (element.tagName.toLowerCase() === "a") {
          element.setAttribute("href", bresUrl);
        } else if (element.tagName.toLowerCase() === "img") {
          element.setAttribute("src", bresUrl);
        } else {
          // Handle other elements that might have resource URLs
          if (element.hasAttribute("href")) {
            element.setAttribute("href", bresUrl);
          }
          if (element.hasAttribute("src")) {
            element.setAttribute("src", bresUrl);
          }
        }

        console.log(`Relative resource URL converted: ${url} -> ${bresUrl}`);
      } else if (dictId) {
        console.warn(
          `Unsafe dictId found in data-gd-id: "${dictId}". Skipping resource URL modification for: ${url}`,
        );
      } else {
        console.warn(
          `Could not find parent .gdarticle element for relative URL: ${url}`,
        );
      }
    }
  }
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

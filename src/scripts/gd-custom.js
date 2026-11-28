// document ready
document.addEventListener("DOMContentLoaded", () => {
  document.addEventListener("click", (event) => {
    const anchor = event.target.closest("a");
    if (!anchor) {
      return;
    }

    const link = anchor.getAttribute("href");
    if (typeof link !== "string") {
      return;
    }

    if (link.includes("javascript:") || link === "#") {
      return;
    }

    // Links containing a colon (gdlookup:, bword:, mailto:, http:, ...) are
    // already absolute. Let them through as-is; the C++ acceptNavigationRequest
    // handler will route internal schemes correctly.
    if (link.includes(":")) {
      return;
    }

    let newLink;
    const { href } = globalThis.location;

    if (link.startsWith("#")) {
      // the href may contain # fragment already. remove them before append the new #fragment
      const index = href.indexOf("#");
      newLink = index > -1 ? href.substring(0, index) + link : href + link;
    } else if (link.indexOf("#") > 0) {
      // if hashtag # is not in the start position, it must be greater than 0
      const index = link.indexOf("#");
      newLink = `gdlookup://localhost/${link.substring(0, index)}?gdanchor=${link.substring(index + 1)}`;
    } else if (link.includes("?gdanchor")) {
      newLink = `gdlookup://localhost/${link}`;
    } else {
      const index = href.indexOf("?");
      const basePath = index > -1 ? href.substring(0, index) : href;
      newLink = `${basePath}?word=${link}`;
    }
    anchor.setAttribute("href", newLink);
  });
});

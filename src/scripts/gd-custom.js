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

    // Links as bword:xxx etc.
    if (link.includes(":")) {
      // Chromium requires an authority (//) for navigations to schemes
      // registered with QWebEngineUrlScheme::Syntax::Host (bword, gdau, ...).
      // Legacy dictionaries emit the opaque form (bword:word); normalize it so
      // the click becomes a real navigation. External schemes (mailto:, ...)
      // are left untouched.
      const scheme = link.slice(0, link.indexOf(":")).toLowerCase();
      const internalSchemes = [
        "bword"
      ];
      if (!link.includes("//") && internalSchemes.includes(scheme)) {
        anchor.setAttribute("href", link.replace(":", "://"));
      }
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

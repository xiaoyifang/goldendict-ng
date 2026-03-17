// document ready
(($) => {
  $(() => {
    $(document).on("click", "a", function () {
      const link = $(this).attr("href");
      if (typeof link !== "string") {
        return;
      }

      if (link.includes("javascript:") || link === "#") {
        return;
      }

      // return if the link is like gdlookup:// or other valid url. bword:xxx is also valid url.
      if (link.includes(":")) {
        emitClickedEvent(link);
        return false;
      }
      emitClickedEvent("");

      let newLink;
      const { href } = window.location;

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
      $(this).attr("href", newLink);
    });

    // monitor iframe height.
    $("iframe").iFrameResize({
      checkOrigin: false,
      maxHeight: 800,
      scrolling: true,
      warningTimeout: 0,
      minHeight: 550,
      log: true,
      autoResize: false,
    });
  });
})(jQuery);

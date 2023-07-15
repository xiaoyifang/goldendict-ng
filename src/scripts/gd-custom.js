//document ready
(function ($) {
  $(function () {
    $(document).on("click", "a", function (event) {
      var link = $(this).attr("href");
      if ("string" != typeof link) {
        return;
      }

      if (link.indexOf("javascript:") >= 0) {
        return;
      }

      //return if the link is like gdlookup:// or other valid url.
      if (link.indexOf(":") >= 0) {
        emitClickedEvent(link);
        return false;
      }
      emitClickedEvent("");

      var newLink;
      var href = window.location.href;
      var index = -1;
      if (link.startsWith("#")) {
        //the href may contain # fragment already.remove them before append the new #fragment
        index = href.indexOf("#");
        if (index > -1) {
          newLink = href.substring(0, index) + link;
        } else {
          newLink = href + link;
        }
      }
      //if hashtag # is not in the start position,it must greater than 0
      else if (link.indexOf("#") > 0) {
        index = link.indexOf("#");
        newLink =
          "gdlookup://localhost/" +
          link.substring(0, index) +
          "?gdanchor=" +
          link.substring(index + 1);
      } else {
        index = href.indexOf("?");

        if (link.indexOf("?gdanchor") > -1) {
          newLink = "gdlookup://localhost/" + link;
        } else {
          if (index > -1) {
            newLink = href.substring(0, index) + "?word=" + link;
          } else {
            newLink = href + "?word=" + link;
          }
        }
      }
      $(this).attr("href", newLink);
    });

    //monitor iframe height.
    $("iframe").iFrameResize({
      checkOrigin: false,
      maxHeight: 800,
      scrolling: true,
      warningTimeout: 0,
      minHeight: 250,
      log: true,
      autoResize: false,
    });
  });
})($_$);

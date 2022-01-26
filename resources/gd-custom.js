//document ready
(function($){
$(function() {
        $("a").click(function(event) {
            var ext = '';
            var link = '';
            if ('string' == typeof($(this).attr("href"))) {
                link = $(this).attr("href");
            } else if ('string' == typeof($(this).attr("src"))) {
                link = $(this).attr("src");
            }
            if ('' == link) {
                return;
            }
            
            ext = link.substring(link.indexOf('.')).toLowerCase();
            if ('.mp3' == ext || '.wav' == ext || '.spx' == ext) {
                playSound(link);
                return;
            }

            emitClickedEvent(link);
            if(link.indexOf(":")>=0){
                return;
            }

            var newLink;
            var href = window.location.href;
            var index=-1;
            if (link.startsWith("#")) {
                //the href may contain # fragment already.remove them before append the new #fragment
                index = href.indexOf("#");
                if(index>-1)
                {
                    newLink = href.substring(0, index) + link;
                } 
                else{
                    newLink= href+link;
                }
            } else {
                index = href.indexOf("?");
                if(index>-1)
                {
                    newLink = href.substring(0, index) + "?word=" + link;
                }
                else{
                    newLink=href+"?word=" + link;
                }
            }
            $(this).attr("href", newLink);

        });

    });
})($_$);

function playSound(sound) {
    var a = new Audio(sound);
    a.play();
}

function emitClickedEvent(link){
    try{
        articleview.linkClickedInHtml(link);
    }catch(error)
    {
        console.error(error);
    }

}

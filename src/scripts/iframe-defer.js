var start_time=new Date().getTime();

var interval = globalThis.setInterval(function () {
    var end_time=new Date().getTime();
    //for 10 seconds.
    if(end_time-start_time>=10000){
        clearInterval(interval);
    }

    const body = document.body;
    const html = document.documentElement;

    const height = Math.max(body.scrollHeight, body.offsetHeight,
    html.clientHeight, html.scrollHeight, html.offsetHeight);

    if ('parentIFrame' in globalThis) {
        console.log("iframe set height to " + height);
        parentIFrame.size(height); 
    }
    else {
        clearInterval(interval);
    }
}, 500);

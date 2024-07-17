POST https://eastus.tts.speech.microsoft.com/cognitiveservices/v1

Ocp-Apim-Subscription-Key: b9885138792d4403a8ccf1a34553351d
X-Microsoft-OutputFormat: audio-16khz-64kbitrate-mono-mp3
Content-Type: application/ssml+xml
User-Agent: WhatEver
<speak version='1.0' xml:lang='en-US'>
    <voice name='en-US-LunaNeural'>
        hello world
    </voice>
</speak>

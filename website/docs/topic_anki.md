# Anki Integration

## Using URI schemes

`goldendict://word` link can be use to query a word directly on Goldendict.

On your Anki card's template, you can add the code below to have a "1 click open in Goldendict" card.

```
<a href="goldendict://{{Front}}">{{Front}}</a>
```

Note that this feature doesn't available on macOS 

## AnkiConnects

See [how to connect with anki.md](https://github.com/xiaoyifang/goldendict/blob/staged/howto/how%20to%20connect%20with%20anki.md)
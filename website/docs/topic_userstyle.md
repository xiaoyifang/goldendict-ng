By creating `article-style.css` or `article-script.js` in GoldenDict's configuration folder (beside the `config` file), you can change dictionaries' presentation or inject javascript to dictionaries.

```
.    <- GD's configuration folder
├── config
├── article-style.css
├── article-style-print.css (affecting styles when printing)
├── article-script.js
└── qt-style.css
```

The `article-style.css` is just standard HTML [Style Sheets](https://developer.mozilla.org/docs/Web/CSS). To know class or id names used in article, you can open inspector by right click article's body and click `Inspect (F12)`. The inspector's documentation can be found at [Chrome DevTools](https://developer.chrome.com/docs/devtools/)

You can adjust dark reader mode's parameter by add those lines to `article-script.js`

```javascript
DarkReader.enable({
    brightness: 100,
    contrast: 100,
    sepia: 0,
    grayscale: 0,
    darkSchemeBackgroundColor: "#181a1b",
    darkSchemeTextColor: "#e8e6e3",
    lightSchemeBackgroundColor: "#dcdad7",
    lightSchemeTextColor: "#181a1b",
});
```

Also, you can tune GoldenDict's interface by creating `qt-style.css` style sheet file in GoldenDict configuration folder. It is a [Qt Style Sheet](https://doc.qt.io/qt-6/stylesheet-reference.html) loaded during startup.

Samples of `article-style.css` and `qt-style.css` files can found in GoldenDict's source code at [/src/stylesheets](https://github.com/xiaoyifang/goldendict-ng/tree/staged/src/stylesheets)

## "Addon" Styles

Under GoldenDict's configuration folder, you can create a "styles" folder for "Addon" styles, so that you can switch between multiple `article-style.css` and `qt-style.css`.

Folder structure like below will create two “addon” styles to switch in settings -> appearances.

```
.    <- GD's configuration folder
├── config
└── styles
    ├── dark
    │   ├── article-style.css
    │   └── qt-style.css
    └── light
        ├── article-style.css
        └── qt-style.css
```


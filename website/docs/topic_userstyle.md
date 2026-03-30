# User Style Customization

By creating `article-style.css` or `article-script.js` in GoldenDict's configuration folder (beside the `config` file), you can change dictionaries' presentation or inject JavaScript to dictionaries.

## Accessing the Configuration Folder

To access GoldenDict's configuration folder, follow these steps based on your operating system:

### Windows

1. Press `Win + R` to open the Run dialog
2. Enter `%APPDATA%\GoldenDict` and press Enter
3. You will see the folder containing the `config` file

### macOS

1. Open Finder
2. Hold down the `Option` key and click the "Go" menu
3. Select "Library"
4. Navigate to `Application Support/GoldenDict`

### Linux

The configuration folder is typically located in one of the following locations:
- `~/.goldendict`
- `~/.config/goldendict`

## Modifying Configuration Files

1. Once you find the configuration folder, you can create or edit the following files:
   - `article-style.css` - Customize dictionary article styles
   - `article-script.js` - Inject JavaScript into dictionary articles
   - `qt-style.css` - Customize GoldenDict's interface styles

2. After saving the files, restart GoldenDict to apply the changes

## Configuration File Structure

GD's configuration folder structure is as follows:

```
.
├── config
├── article-style.css
├── article-style-print.css (affecting styles when printing)
├── article-script.js
└── qt-style.css
```

## Dictionary Style Customization

### article-style.css

`article-style.css` is a standard HTML [Style Sheet](https://developer.mozilla.org/docs/Web/CSS). To know class or id names used in article, you can open inspector by right click article's body and click `Inspect (F12)`. The inspector's documentation can be found at [Chrome DevTools](https://developer.chrome.com/docs/devtools/).

#### Common Article Style Customization Examples

Here are some common article-style.css examples:

```css
/* Change article background color */
body {
  background-color: #f5f5f5;
  color: #333;
}

/* Change heading styles */
h1, h2, h3 {
  color: #2c3e50;
  font-family: Arial, sans-serif;
  margin-top: 1em;
  margin-bottom: 0.5em;
}

/* Change dictionary entry style */
.article {
  padding: 10px;
  border-radius: 5px;
  background-color: #fff;
  box-shadow: 0 1px 3px rgba(0,0,0,0.1);
}

/* Change link style */
a {
  color: #3498db;
  text-decoration: none;
}

a:hover {
  text-decoration: underline;
}

/* Change audio button style */
.playSound {
  background-color: #e74c3c;
  color: white;
  border: none;
  border-radius: 3px;
  padding: 2px 6px;
  cursor: pointer;
}

.playSound:hover {
  background-color: #c0392b;
}

/* Change definition list style */
dl {
  margin: 0;
  padding: 0;
}

dt {
  font-weight: bold;
  margin-top: 0.5em;
}

dd {
  margin-left: 1em;
  margin-bottom: 0.5em;
}

/* Change example sentences style */
.example {
  font-style: italic;
  color: #7f8c8d;
  margin-left: 1em;
}
```

### qt-style.css

You can also tune GoldenDict's interface by creating `qt-style.css` style sheet file in GoldenDict configuration folder. It is a [Qt Style Sheet](https://doc.qt.io/qt-6/stylesheet-reference.html) loaded during startup.

#### Common Interface Style Customization Examples

Here are some common qt-style.css examples:

```css
/* Change main window background */
QMainWindow {
  background-color: #f0f0f0;
}

/* Change toolbar style */
QToolBar {
  background-color: #333;
  border: none;
  padding: 4px;
}

/* High contrast toolbar buttons */
QToolBar QPushButton {
  background-color: #4CAF50;
  color: white;
  border: 2px solid #388E3C;
  border-radius: 4px;
  padding: 6px 12px;
  font-weight: bold;
  min-width: 80px;
}

QToolBar QPushButton:hover {
  background-color: #45a049;
  border-color: #2E7D32;
}

QToolBar QPushButton:pressed {
  background-color: #388E3C;
  border-color: #1B5E20;
}

/* Change general button style */
QPushButton {
  background-color: #3498db;
  color: white;
  border: none;
  border-radius: 3px;
  padding: 5px 10px;
  font-weight: bold;
}

QPushButton:hover {
  background-color: #2980b9;
}

/* Change input box style */
QLineEdit {
  border: 1px solid #bdc3c7;
  border-radius: 3px;
  padding: 5px;
  background-color: white;
}

/* Change list and tree view style */
QListWidget, QTreeWidget {
  background-color: white;
  border: 1px solid #bdc3c7;
  border-radius: 3px;
}

QListWidget::item:selected, QTreeWidget::item:selected {
  background-color: #bbdefb;
  color: #2c3e50;
}
```

### article-script.js

`article-script.js` is a JavaScript file used to inject custom scripts into dictionary articles, enabling more complex interactive functionality.

#### Features and Uses

- Add interactive functionality to dictionary articles
- Modify article content or structure
- Implement custom search or navigation features
- Integrate external services or APIs

#### Example Code

Here are some common article-script.js examples:

```javascript
// Execute when page is loaded
window.addEventListener('DOMContentLoaded', function() {
  // Example 1: Add click effect to all audio buttons
  const audioButtons = document.querySelectorAll('.playSound');
  audioButtons.forEach(button => {
    button.addEventListener('click', function() {
      // Add click animation effect
      this.style.transform = 'scale(0.95)';
      setTimeout(() => {
        this.style.transform = 'scale(1)';
      }, 100);
    });
  });

  // Example 2: Add new window opening functionality to all links
  const links = document.querySelectorAll('a');
  links.forEach(link => {
    link.setAttribute('target', '_blank');
    link.setAttribute('rel', 'noopener noreferrer');
  });

  // Example 3: Add custom toolbar
  const toolbar = document.createElement('div');
  toolbar.style.cssText = `
    position: fixed;
    top: 10px;
    right: 10px;
    background: rgba(255, 255, 255, 0.9);
    padding: 5px;
    border-radius: 5px;
    box-shadow: 0 2px 5px rgba(0,0,0,0.2);
    z-index: 1000;
  `;
  
  const button = document.createElement('button');
  button.textContent = 'Toggle Theme';
  button.style.cssText = `
    padding: 5px 10px;
    border: none;
    border-radius: 3px;
    background: #3498db;
    color: white;
    cursor: pointer;
  `;
  
  button.addEventListener('click', function() {
    document.body.classList.toggle('dark-theme');
  });
  
  toolbar.appendChild(button);
  document.body.appendChild(toolbar);
});

// Example 4: Define global function for other scripts to use
function highlightText(text) {
  const elements = document.querySelectorAll('p, span, div');
  elements.forEach(element => {
    if (element.textContent.includes(text)) {
      element.innerHTML = element.innerHTML.replace(
        new RegExp(text, 'gi'),
        '<mark style="background-color: yellow;">$&</mark>'
      );
    }
  });
}
```

### Sample Files

Samples of `article-style.css` and `qt-style.css` files can be found in GoldenDict's source code at [/src/stylesheets](https://github.com/xiaoyifang/goldendict-ng/tree/staged/src/stylesheets).

## "Addon" Styles

Under GoldenDict's configuration folder, you can create a "styles" folder for "Addon" styles, so that you can switch between multiple `article-style.css` and `qt-style.css`.

The following folder structure will create two "addon" styles to switch in settings -> appearances:

```
.
├── config
└── styles
    ├── dark
    │   ├── article-style.css
    │   └── qt-style.css
    └── light
        ├── article-style.css
        └── qt-style.css
```

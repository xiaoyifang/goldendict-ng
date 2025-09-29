# Website Scripting

GoldenDict allows you to inject custom JavaScript code into websites opened in separate tabs. This feature is useful for customizing the appearance or behavior of websites, fixing display issues, or adding additional functionality.

## How to Use Website Scripting

1. Open the **Edit | Dictionaries** dialog:
2. Go to the **Websites** tab:
3. Add or edit a website entry:
4. In the **Script** column, you can specify either:
    - A file path (relative to the config directory or absolute)
    - Direct JavaScript code

## Script Column Details

The **Script** column in the Websites configuration accepts two types of input:

1. **File Path:**
    - **Relative path:** Path relative to the GoldenDict configuration directory
    - **Absolute path:** Full path to a JavaScript file on your system
   
2. **Direct Script Content:**
    - JavaScript code directly entered in the field

When you specify a file path, GoldenDict will read the content of that file and inject it as JavaScript into the matching website. If the file doesn't exist or you've entered JavaScript code directly, GoldenDict will inject the content of the field directly as JavaScript.

## Configuration Directory

The configuration directory location depends on your operating system:

- **Windows**: `%APPDATA%\GoldenDict\`
- **Linux**: `~/.goldendict/` or `~/.config/goldendict/` (depending on installation)
- **macOS**: `~/Library/Preferences/GoldenDict/`

You can also check the menu "Help->Configuration folder" to see the exact configuration directory path.

## Example Usage

### Using a File

1. Create a JavaScript file, for example `mywebsite.js` in your config directory:
2. Add your JavaScript code to this file:
   ```javascript
   // Example: Hide a specific element
   document.addEventListener('DOMContentLoaded', function() {
       var element = document.getElementById('annoying-banner');
       if (element) {
           element.style.display = 'none';
       }
   });
   ```
3. In the Script column, enter: `mywebsite.js`:

### Using Direct Script Content

Enter JavaScript code directly in the Script column:
```javascript
document.addEventListener('DOMContentLoaded', function() {
    document.body.style.backgroundColor = '#f0f0f0';
});
```

## Important Notes

- Scripts are only executed when websites are opened in separate tabs, not in the main window
- The script is injected after the page loads, so use `DOMContentLoaded` event for DOM manipulations
- Be careful with the scripts you inject, as they have the same permissions as the website itself
- If using file paths, make sure the files exist and are readable by GoldenDict
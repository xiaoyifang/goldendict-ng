# Website Whitelist

GoldenDict-ng by default blocks external content (like cross-domain scripts, IFrames, etc.) from websites loaded in the article view for security reasons. However, some websites require these resources to function correctly (e.g., Google Translate loading fonts or scripts from `gstatic.com`).

The `whitelist` parameter allows you to tell GoldenDict-ng to trust a specific website and allow it to load its sub-resources.

## How it works

When you add the `whitelist` parameter to a website's URL template:

1.  GoldenDict-ng adds the website's host to a **Referer Whitelist**.
2.  Any request initiated by this website (where the `Referer` matches the whitelisted host) will be allowed by the internal WebEngine interceptor.

## When to use it?

If a website looks broken (missing icons, styles, or non-working buttons), you can diagnose it:
1. Right-click on the website in GoldenDict-ng and select **Inspect (F12)**.
2. Check the **Console** tab.
3. If you see errors like `ERR_BLOCKED_BY_CLIENT` or `CORS policy` blocking resources from other domains, you need to enable the whitelist.

## How to enable

Append `whitelist=1` (or any value) to your website URL template in the **Edit | Dictionaries -> Websites** tab.

!!! tip "Display Modes"
    The `whitelist` feature is particularly useful when websites are **embedded** in the article view (default behavior). In this mode, GoldenDict-ng's security policy is most restrictive. If you encounter blocked resources, the whitelist can resolve them.
    
    If you use the **Open in New Tab** mode (configured in Preferences), the website has more freedom, but the whitelist still helps GoldenDict-ng identify trusted domains.

### Examples

| Dictionary Name | URL Template                                                                 |
|-----------------|------------------------------------------------------------------------------|
| Google Translate| `https://translate.google.com/?sl=auto&tl=zh-CN&text=%GDWORD%&op=translate&whitelist=1` |
| Wikipedia       | `https://en.wikipedia.org/wiki/%GDWORD%?whitelist=true`                      |

### Global Whitelist File (Advanced)

You can also define trusted hosts globally by creating a plain text file named `whitelist` in your configuration folder (the same directory where the `config` file is located).

*   **Format**: One host per line (e.g., `gstatic.com`).
*   **Negation (Blacklist)**: If a line starts with `-`, that specific host will be **blocked**, even if it matches a broader whitelist rule. 
    *   Example: Adding `google.com` allows all Google subdomains, but adding `-doubleclick.net` will explicitly block that domain.

## Security Note

Only use the `whitelist` parameter for websites you trust. Enabling it allows the website to load third-party scripts and resources that would otherwise be blocked by GoldenDict-ng's security policies.

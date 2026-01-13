# Website Whitelist

GoldenDict-ng by default blocks external content (like cross-domain scripts, IFrames, etc.) from websites loaded in the article view for security reasons. However, some websites require these resources to function correctly (e.g., Google Translate loading fonts or scripts from `gstatic.com`).

The `whitelist` parameter allows you to tell GoldenDict-ng to trust a specific website and allow it to load its sub-resources.

## How it works

When you add the `whitelist` parameter to a website's URL template:

1.  GoldenDict-ng adds the website's host to a **Referer Whitelist**.
2.  Any request initiated by this website (where the `Referer` matches the whitelisted host) will be allowed by the internal WebEngine interceptor.


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

## Security Note

Only use the `whitelist` parameter for websites you trust. Enabling it allows the website to load third-party scripts and resources that would otherwise be blocked by GoldenDict-ng's security policies.

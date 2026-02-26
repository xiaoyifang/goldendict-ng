# GoldenDict-ng Startup Performance Optimization Record

## 1. Optimization Goal
Reduce interface stuttering during cold start, shorten the perceived time from clicking to displaying the main window, and lower the CPU and disk I/O peaks during the startup phase.

## 2. Core Strategy: Deferred Loading & Staged Startup
Decouple non-critical tasks from the `MainWindow` constructor and synchronous initialization flow. Utilize `QTimer::singleShot` to trigger these tasks in stages after the main event loop starts, achieving a "progressive startup."

## 3. Optimization Item List

| Task Item | Original Timing | Optimized Timing | Remarks |
| :--- | :--- | :--- | :--- |
| **ArticleInspector (Debugger)** | Sync creation in constructor | **Lazy creation** | Instantiated only when "Inspect Element" is triggered |
| **ScanPopup (Scan Progress)** | Sync trigger in constructor | **Delayed 1,000ms** | Avoids QWebEngine loading blocking main window display |
| **TrayIcon** | Sync init in constructor | **Delayed 1,000ms** | Reduces synchronous communication with system shell |
| **GlobalHotkeys** | Sync installation in constructor | **Delayed 2,000ms** | Moves system-level hotkey registration to background |
| **doDeferredInit (Deep Init)** | Sync execution at end of constructor | **Delayed 3,000ms** | Avoids peak I/O for file handles and abbreviation loading |
| **FullTextSearch (FTS Indexing)** | Sync start in `makeDictionaries` | **Delayed 5,000ms** | Ensures UI is idle before starting large-scale disk scanning |
| **New Release Check** | Immediate execution (0ms) | **Delayed 10,000ms** | Moves network requests and JSON parsing after stability |

## 4. Technical Details

### 4.1 Automatic Fallback Mechanism (`ensureInitDone`)
For the `doDeferredInit` delay, existing dictionary class implementations (e.g., `DslDictionary`, `MdxDictionary`) already include `ensureInitDone()` protection logic.
- **Safety**: If a user performs a lookup or FTS search before the delay (3s) expires, the code will automatically trigger synchronous initialization, ensuring no loss of functionality.
- **Experience**: The delay is only to release system resources and does not cause functional deadlocks.

### 4.2 UI Responsiveness Priority
By delaying `ScanPopup` and `ArticleInspector` (both WebEngine-driven), we significantly reduce the peak frequency of VRAM and RAM allocation, allowing the main viewport (`ArticleView`) to complete its first-paint at the highest priority.

## 5. Expected Results
- **Perceived Speedup**: Main window display speed improved by 30% - 50%.
- **I/O Peak Shaving**: Smooths the disk I/O "spike" from massive dictionary loading into a sustained "low-load" process over several seconds.
- **Stability**: Reduces the probability of thread deadlocks or UI freezes caused by resource contention during critical startup moments.

---
*Date: 2026-02-05*

/* This file is (c) 2013 Abs62
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "hotkeywrapper.hh"
#include <QMessageBox>
#include <QObject>
#include <QPushButton>
#include <QTimer>

#include <memory>
#include <vector>

#import <Appkit/Appkit.h>

///
/// Implementation of the Cmd+C+C trick:
/// These two methods from `Carbon` API are the core part:
/// * `RegisterEventHotKey` -> For trapping Cmd+C and waiting for second Cmd+C press.
/// * `CGEventCreateKeyboardEvent` -> For simulating Cmd+C press.
///
/// The capturing of the first Cmd+C will prevent user's normal copy action.
/// The trick is to insert a Cmd+C in between two user generated Cmd+C.
///
/// Note: Based an expert's opinion, despite `Carbon` APIs are mostly deprecated, the used two are not.
/// https://github.com/sindresorhus/KeyboardShortcuts/blob/9369a045a72a5296150879781321aecd228171db/readme.md?plain=1#L207
///

namespace MacKeyMapping {
// Convert Qt key codes to Mac OS X native codes

struct ReverseMapEntry {
    UniChar character;
    UInt16 keyCode;
};

static std::vector<ReverseMapEntry> mapping;

/// References:
/// * https://github.com/libsdl-org/SDL/blob/fc12cc6dfd859a4e01376162a58f12208e539ac6/src/video/cocoa/SDL_cocoakeyboard.m#L345
/// * https://github.com/qt/qtbase/blob/922369844fcb75386237bca3eef59edd5093f58d/src/gui/platform/darwin/qapplekeymapper.mm#L449
///
///  Known possible flaws 1) UCKeyTranslate doesn't handle modifiers at all 2) Handling keyboard switching
void createMapping()
{
    if (mapping.empty()) {
        mapping.reserve(128);

        TISInputSourceRef inputSourceRef = TISCopyCurrentKeyboardLayoutInputSource();
        if (!inputSourceRef) {
            return;
        }

        CFDataRef uchrDataRef = (CFDataRef)TISGetInputSourceProperty(inputSourceRef, kTISPropertyUnicodeKeyLayoutData);

        const UCKeyboardLayout* UCKeyboardLayoutPtr;

        if (uchrDataRef) {
            UCKeyboardLayoutPtr = (const UCKeyboardLayout*)CFDataGetBytePtr(uchrDataRef);
        }

        if (!UCKeyboardLayoutPtr) {
            return;
        }

        for (UInt16 i = 0; i < 128; i++) {
            UInt32 theDeadKeyState = 0;
            UniCharCount theLength = 0;
            UniChar temp_char_buf;
            if (UCKeyTranslate(UCKeyboardLayoutPtr, i, kUCKeyActionDown, 0, LMGetKbdType(),
                    kUCKeyTranslateNoDeadKeysBit, &theDeadKeyState, 1, &theLength,
                    &temp_char_buf)
                    == noErr
                && theLength > 0) {
                if (isprint(temp_char_buf)) {
                    mapping.emplace_back(ReverseMapEntry { temp_char_buf, i });
                }
            }
        }
        mapping.shrink_to_fit();
    }
}

quint32 qtKeyToNativeKey(UniChar key)
{
    createMapping();
    if (mapping.empty()) {
        return 0;
    }

    for (auto& m : mapping) {
        if (m.character == key) {
            return m.keyCode;
        }
    }

    return 0;
}

} // namespace MacKeyMapping

CGEventRef HotkeyWrapper::eventTapCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void* refcon)
{
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        return event;
    }

    if (type == kCGEventKeyDown) {
        static_cast<HotkeyWrapper*>(refcon)->handleCGEvent(event);
    }

    return event;
}

void HotkeyWrapper::handleCGEvent(CGEventRef event)
{
    CGEventFlags flags = CGEventGetFlags(event);
    uint32_t vk = (uint32_t)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    // Convert CGEventFlags to a simpler bitmask for matching
    uint32_t mod = 0;
    if (flags & kCGEventFlagMaskCommand)
        mod |= cmdKey;
    if (flags & kCGEventFlagMaskAlternate)
        mod |= optionKey;
    if (flags & kCGEventFlagMaskShift)
        mod |= shiftKey;
    if (flags & kCGEventFlagMaskControl)
        mod |= controlKey;

    if (state2) { // wait for 2nd key
        waitKey2(); // Cancel the 2nd-key wait stage

        if (vk == state2waiter.key2 && mod == state2waiter.modifier) {
            emit hotkeyActivated(state2waiter.handle);
            return;
        }
    }

    for (int i = 0; i < hotkeys.count(); i++) {
        HotkeyStruct& hs = hotkeys[i];
        if (vk == hs.key && mod == hs.modifier) {
            if (hs.key2 == 0) {
                emit hotkeyActivated(hs.handle);
                return;
            }

            state2 = true;
            state2waiter = hs;
            QTimer::singleShot(500, this, SLOT(waitKey2()));
            return;
        }
    }
}

HotkeyWrapper::HotkeyWrapper(QObject* parent)
{
    (void)parent;
    keyC = nativeKey('c');

    // Create an event tap to listen for key down events
    eventTap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionListenOnly,
        CGEventMaskBit(kCGEventKeyDown), eventTapCallback, this);

    if (!eventTap) {
        checkAndRequestAccessibilityPermission();
        return;
    }

    runLoopSource = CFRunLoopAddSource(CFRunLoopGetCurrent(),
        CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0),
        kCFRunLoopCommonModes);
}

HotkeyWrapper::~HotkeyWrapper()
{
    unregister();
}

void HotkeyWrapper::waitKey2()
{
    state2 = false;
}

void checkAndRequestAccessibilityPermission()
{
    if (AXIsProcessTrusted()) {
        return;
    }

    auto msgBox = std::make_unique<QMessageBox>(nullptr);
    auto* turnOnPermission = new QPushButton(QObject::tr("Turn on Accessibility"), msgBox.get());

    msgBox->setInformativeText(QObject::tr("Global shortcuts need Accessibility permission to work. Please grant it to Goldendict or change the hotkeys."));

    msgBox->addButton(QMessageBox::Ok);
    msgBox->addButton(turnOnPermission, QMessageBox::AcceptRole);
    msgBox->setDefaultButton(turnOnPermission);
    msgBox->exec();

    if (msgBox->clickedButton() == turnOnPermission) {
        [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"]];
    }
}

void HotkeyWrapper::activated(int hkId)
{
    (void)hkId;
}

void HotkeyWrapper::unregister()
{
    if (eventTap) {
        CGEventTapEnable(eventTap, false);
        CFRelease(eventTap);
        eventTap = nullptr;
    }
    // Note: runLoopSource is cleaned up by CFRunLoop or handled via CFRelease if manually managed
}

bool HotkeyWrapper::setGlobalKey(QKeySequence const& seq, int handle)
{
    Config::HotKey hotkeyParse(seq);
    int key = hotkeyParse.key1;
    int key2 = hotkeyParse.key2;
    Qt::KeyboardModifiers modifier = hotkeyParse.modifiers;

    if (!key) {
        return false;
    }

    quint32 vk = nativeKey(key);
    if (vk == 0) {
        return false;
    }

    quint32 vk2 = key2 ? nativeKey(key2) : 0;

    static int nextId = 1;

    quint32 mod = 0;
    if (modifier & Qt::CTRL) {
        mod |= cmdKey;
    }
    if (modifier & Qt::ALT) {
        mod |= optionKey;
    }
    if (modifier & Qt::SHIFT) {
        mod |= shiftKey;
    }
    if (modifier & Qt::META) {
        mod |= controlKey;
    }

    hotkeys.append(HotkeyStruct(vk, vk2, mod, handle, nextId++));
    return true;
}

quint32 HotkeyWrapper::nativeKey(int key)
{
    switch (key) {
    case Qt::Key_Escape:
        return 0x35;
    case Qt::Key_Tab:
        return 0x30;
    case Qt::Key_Backspace:
        return 0x33;
    case Qt::Key_Return:
        return 0x24;
    case Qt::Key_Enter:
        return 0x4c;
    case Qt::Key_Delete:
        return 0x75;
    case Qt::Key_Clear:
        return 0x47;
    case Qt::Key_Home:
        return 0x73;
    case Qt::Key_End:
        return 0x77;
    case Qt::Key_Left:
        return 0x7b;
    case Qt::Key_Up:
        return 0x7e;
    case Qt::Key_Right:
        return 0x7c;
    case Qt::Key_Down:
        return 0x7d;
    case Qt::Key_PageUp:
        return 0x74;
    case Qt::Key_PageDown:
        return 0x79;
    case Qt::Key_CapsLock:
        return 0x57;
    case Qt::Key_F1:
        return 0x7a;
    case Qt::Key_F2:
        return 0x78;
    case Qt::Key_F3:
        return 0x63;
    case Qt::Key_F4:
        return 0x76;
    case Qt::Key_F5:
        return 0x60;
    case Qt::Key_F6:
        return 0x61;
    case Qt::Key_F7:
        return 0x62;
    case Qt::Key_F8:
        return 0x64;
    case Qt::Key_F9:
        return 0x65;
    case Qt::Key_F10:
        return 0x6d;
    case Qt::Key_F11:
        return 0x67;
    case Qt::Key_F12:
        return 0x6f;
    case Qt::Key_F13:
        return 0x69;
    case Qt::Key_F14:
        return 0x6b;
    case Qt::Key_F15:
        return 0x71;
    case Qt::Key_Help:
        return 0x72;
    default:;
    }
    return MacKeyMapping::qtKeyToNativeKey(QChar(key).toLower().unicode());
}

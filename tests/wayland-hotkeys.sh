#!/usr/bin/env bash
# 验证 GoldenDict-ng 在原生 Wayland 下是否把全局热键注册到 KDE KGlobalAccel。
#
# 用法: ./wayland-hotkeys.sh /path/to/goldendict
# 退出码: 0=PASS, 1=FAIL, 77=SKIP（非 Wayland / 缺 qdbus6）
#
# 注意：测试会先结束正在运行的 goldendict，再用给定二进制启动一个实例；
#       测试结束会结束该实例。请自行重启日常实例。
set -euo pipefail

BIN=${1:?usage: $0 /path/to/goldendict}
SERVICE=org.kde.kglobalaccel
IFACE=org.kde.kglobalaccel.Component
HOTKEY=goldendict_hotkey_0

if [[ ${XDG_SESSION_TYPE:-} != wayland ]]; then
  echo "SKIP: not a Wayland session"
  exit 77
fi
command -v qdbus6 >/dev/null 2>&1 || { echo "SKIP: qdbus6 not found"; exit 77; }
[[ -x $BIN ]] || { echo "FAIL: $BIN is not executable"; exit 1; }

component_paths() {
  qdbus6 --literal "$SERVICE" /kglobalaccel org.kde.KGlobalAccel.allComponents 2>/dev/null \
    | grep -o '/component/[A-Za-z0-9_./-]*' \
    | sort -u
}

component_has_hotkey() {
  local path=$1
  qdbus6 "$SERVICE" "$path" "$IFACE.shortcutNames" 2>/dev/null | grep -qx "$HOTKEY"
}

find_hotkey_component() {
  local path
  for path in $(component_paths); do
    if component_has_hotkey "$path"; then
      printf '%s\n' "$path"
      return 0
    fi
  done
  return 1
}

# 停掉已有实例，避免单实例 IPC 把新进程当成参数转发。
# pkill 返回 1 表示本来就没有进程，属正常；其它返回码才是错误。
if pkill -x goldendict; then
  :
else
  pkill_rc=$?
  if [[ $pkill_rc -ne 1 ]]; then
    echo "FAIL: pkill returned $pkill_rc"
    exit 1
  fi
fi

# 等旧实例的 hotkey 组件从 kglobalaccel 注销；最多 5 秒。
deadline=$((SECONDS + 5))
while :; do
  if ! find_hotkey_component >/dev/null 2>&1; then
    break
  fi
  if (( SECONDS >= deadline )); then
    echo "FAIL: stale goldendict hotkey component still registered"
    exit 1
  fi
  # timeout-ok: 轮询外部 kglobalaccel 注销旧组件，事件不可直接订阅
  sleep 0.1
done

LOG=$(mktemp)
env -u GOLDENDICT_FORCE_XCB -u QT_QPA_PLATFORM "$BIN" >"$LOG" 2>&1 &
PID=$!
cleanup() {
  if kill "$PID" 2>/dev/null; then
    :
  fi
  if wait "$PID" 2>/dev/null; then
    :
  fi
}
trap cleanup EXIT

# 轮询 kglobalaccel 的组件注册；最多 10 秒。
deadline=$((SECONDS + 10))
while :; do
  if component_path=$(find_hotkey_component); then
    infos=$(qdbus6 --literal "$SERVICE" "$component_path" "$IFACE.allShortcutInfos" 2>/dev/null)
    if printf '%s' "$infos" | grep -q '"goldendict_hotkey_[01]", ""'; then
      echo "FAIL: shortcut friendly names are empty in $component_path"
      exit 1
    fi
    echo "PASS: $BIN registered $HOTKEY under $component_path on Wayland"
    exit 0
  fi
  if (( SECONDS >= deadline )); then
    echo "FAIL: $BIN did not register $HOTKEY on Wayland"
    tail -20 "$LOG"
    exit 1
  fi
  # timeout-ok: 轮询外部 kglobalaccel 接收新实例注册，事件不可直接订阅
  sleep 0.25
done

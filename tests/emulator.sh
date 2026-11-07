#!/usr/bin/env bash
# Installs the real APK on an emulator, drives it with adb input (taps on the
# launcher art, the on-screen game controls and the Android back key) and
# captures screenshots + logcat. Fails if the game process crashes or if Back
# closes the game.
#   tests/emulator.sh <apk> <out dir>
set -uo pipefail
APK="$1"; OUT="$2"
PKG=be.liero.mobile
mkdir -p "$OUT"
adb wait-for-device
adb shell settings put global window_animation_scale 0
adb shell settings put system accelerometer_rotation 0
adb install -r "$APK" || exit 1
adb logcat -c

shot() { adb exec-out screencap -p > "$OUT/$1.png"; echo "shot $1"; }
gamepid() { adb shell pidof "$PKG:game" | tr -d '\r'; }

# Landscape size the app sees
read PW PH < <(adb shell wm size | sed -n 's/.*: \([0-9]*\)x\([0-9]*\).*/\1 \2/p' | tail -1)
W=$(( PW > PH ? PW : PH )); H=$(( PW > PH ? PH : PW ))
echo "screen ${W}x${H}"

# Launcher art (1672x941) is drawn "fit" and centred
art() { # $1 $2 = art coords -> prints screen coords
  python3 - "$1" "$2" "$W" "$H" <<'PY'
import sys
x,y,W,H=map(float,sys.argv[1:])
s=min(W/1672,H/941); ox=(W-1672*s)/2; oy=(H-941*s)/2
print(int(ox+x*s), int(oy+y*s))
PY
}

adb shell am start -W -n $PKG/.LauncherActivity
sleep 4
shot 01_menu

# Loading screen (frozen at 62% for the screenshot)
adb shell am force-stop $PKG
adb shell am start -W -n $PKG/.LauncherActivity --ez screenshot_loading true
sleep 3
shot 02_loading
adb shell am force-stop $PKG

# Settings + stats screens
adb shell am start -W -n $PKG/.LauncherActivity; sleep 3
read X Y < <(art 836 676); adb shell input tap $X $Y; sleep 2; shot 03_personalizar
adb shell input keyevent KEYCODE_BACK; sleep 1
read X Y < <(art 836 781); adb shell input tap $X $Y; sleep 2; shot 04_mapas
adb shell input keyevent KEYCODE_BACK; sleep 1
read X Y < <(art 1598 65); adb shell input tap $X $Y; sleep 2; shot 05_conquistas
adb shell input keyevent KEYCODE_BACK; sleep 1

# JOGAR
read X Y < <(art 836 433); adb shell input tap $X $Y
for i in $(seq 1 30); do sleep 1; [ -n "$(gamepid)" ] && break; done
sleep 6
shot 06_weapon_select
PID1=$(gamepid)
echo "game pid: $PID1"

layout() { adb logcat -d -s SDL/APP:I | grep 'liero-layout mode=1' | tail -1; }
L=$(layout); echo "$L"
val() { echo "$L" | sed -n "s/.* $1=\([0-9-]*\),\([0-9-]*\).*/\1 \2/p"; }
read SX SY < <(val stick)
SR=$(echo "$L" | sed -n 's/.* stick=[0-9]*,[0-9]*,\([0-9]*\).*/\1/p')
read FX FY < <(val b4)   # VFire
read JX JY < <(val b5)   # VJump
read RX RY < <(val b7)   # VRope
read WX WY < <(val b8)   # VSwapTap
read PX PY < <(val pause)
echo "stick=$SX,$SY r=$SR fire=$FX,$FY rope=$RX,$RY swap=$WX,$WY pause=$PX,$PY"

# Weapon menu: stick down x6 to DONE, then FIRE
for i in 1 2 3 4 5 6; do adb shell input swipe $SX $SY $SX $((SY + SR / 2)) 120; sleep 0.3; done
adb shell input tap $FX $FY
sleep 3
shot 07_match

# Hold fire while walking (two concurrent long-presses)
adb shell input swipe $SX $SY $((SX + SR / 2)) $SY 2500 &
adb shell input swipe $FX $FY $FX $FY 2000 &
sleep 1.2; shot 08_firing; wait

adb shell input tap $WX $WY; sleep 0.4; shot 09_weapon_swap
sleep 1
adb shell input swipe $SX $SY $SX $((SY - SR / 2)) 1500 &
sleep 0.5
adb shell input swipe $RX $RY $RX $RY 1500 &
sleep 1.0; shot 10_ninja_rope; wait

# Pause button
adb shell input tap $PX $PY; sleep 2; shot 11_pause_menu
# Android Back must NOT close the game
adb shell input keyevent KEYCODE_BACK; sleep 1.5
adb shell input keyevent KEYCODE_BACK; sleep 1.5
shot 12_after_back
PID2=$(gamepid)

# Background / foreground keeps state
adb shell input keyevent KEYCODE_HOME; sleep 2
adb shell monkey -p $PKG -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1   # like tapping the icon
sleep 3
shot 13_after_home
PID3=$(gamepid)

adb logcat -d > "$OUT/logcat.txt"
grep -E 'liero-fps|liero-layout|liero:' "$OUT/logcat.txt" | tail -20

status=0
if [ -z "$PID1" ]; then echo "ERROR: game process never started"; status=1; fi
if [ "$PID1" != "$PID2" ]; then echo "ERROR: game process changed/died after Back ($PID1 -> $PID2)"; status=1; fi
if [ "$PID1" != "$PID3" ]; then echo "WARN: game process changed after Home ($PID1 -> $PID3)"; fi
if grep -E "FATAL EXCEPTION|Fatal signal|liero: fatal" "$OUT/logcat.txt" | grep -v "^--"; then echo "ERROR: crash in logcat"; status=1; fi
exit $status

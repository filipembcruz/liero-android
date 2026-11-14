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
calc() { python3 -c "W=$W;H=$H;s=H/720.0;$1"; }

# Launcher buttons are drawn by the app (unit u = H/720), see LauncherActivity.layoutButtons
btn() { # $1 = play|custom|maps|trophy -> prints screen coords
  python3 - "$1" "$W" "$H" <<'PY'
import sys
k,W,H=sys.argv[1],float(sys.argv[2]),float(sys.argv[3]); u=H/720
y0=H*0.47
pos={'play':(W/2,y0+46*u),'custom':(W/2,y0+92*u+22.4*u+84*u+35*u),'maps':(W/2,y0+92*u+22.4*u+168*u+35*u),'trophy':(W-24*u-36*u,18*u+36*u)}
x,y=pos[k]; print(int(x),int(y))
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
read X Y < <(btn custom); adb shell input tap $X $Y; sleep 2; shot 03_personalizar
# compact panel tabs (PanelView): 4 tabs right-aligned in the panel header
for T in 1 2 3; do
  read TX TY < <(calc "u=H/720; pw=min(1120*u, W-40*u); right=(W+pw)/2; print(int(right-24*u-(4-$T)*176*u+10*u-83*u), int(30*u+45*u))")
  adb shell input tap $TX $TY; sleep 1; shot 03_personalizar_tab$T
done
adb shell input keyevent KEYCODE_BACK; sleep 1
read X Y < <(btn maps); adb shell input tap $X $Y; sleep 2; shot 04_mapas
adb shell input keyevent KEYCODE_BACK; sleep 1
read X Y < <(btn trophy); adb shell input tap $X $Y; sleep 2; shot 05_conquistas
adb shell input keyevent KEYCODE_BACK; sleep 1

# JOGAR
read X Y < <(btn play); adb shell input tap $X $Y
for i in $(seq 1 30); do sleep 1; [ -n "$(gamepid)" ] && break; done
sleep 6
shot 06_weapon_picker
PID1=$(gamepid)
echo "game pid: $PID1"

# Native weapon picker: ALEATÓRIO, then JOGAR
read GX GY < <(calc "sw=min(200*s,(W-620*s)/5); bx=30*s+sw*5+16*s+24*s; bw=W-bx-30*s; print(int(bx+bw/2), int(H-176*s+115*s))")
adb shell input tap $GX $((GY - 76 * H / 720)); sleep 1.5; shot 07_weapon_picker_random
adb shell input tap $GX $GY
sleep 5
shot 08_match_hud

layout() { adb logcat -d -s SDL/APP:I | grep 'liero-layout mode=1' | tail -1; }
L=$(layout); echo "$L"
val() { echo "$L" | sed -n "s/.* $1=\([0-9-]*\),\([0-9-]*\).*/\1 \2/p"; }
read SX SY < <(val stick)
SR=$(echo "$L" | sed -n 's/.* stick=[0-9]*,[0-9]*,\([0-9]*\).*/\1/p')
read FX FY < <(val b4)   # VFire
read RX RY < <(val b7)   # VRope
read WX WY < <(val b8)   # VSwapTap
echo "stick=$SX,$SY r=$SR fire=$FX,$FY rope=$RX,$RY swap=$WX,$WY"

# Hold fire while walking (two concurrent long-presses)
adb shell input swipe $SX $SY $((SX + SR / 2)) $SY 2500 &
adb shell input swipe $FX $FY $FX $FY 2000 &
sleep 1.2; shot 09_firing; wait

adb shell input tap $WX $WY; sleep 0.6; shot 10_weapon_swap
sleep 1
adb shell input swipe $SX $SY $SX $((SY - SR / 2)) 1500 &
sleep 0.5
adb shell input swipe $RX $RY $RX $RY 1500 &
sleep 1.0; shot 11_ninja_rope; wait

# HUD pause button (next to the minimap) -> native pause menu
read PX PY < <(calc "mpx=W-270*s-16*s-6*s; print(int(mpx-70*s+29*s), int(43*s))")
adb shell input tap $PX $PY; sleep 2.5; shot 12_pause_menu
# Settings (3rd button)
read MX MY < <(calc "ph=520*s; py=(H-ph)/2; print(int(W/2), int(py+140*s+2*78*s+31*s))")
adb shell input tap $MX $MY; sleep 1.5; shot 13_settings
adb shell input keyevent KEYCODE_BACK; sleep 1.5
# Android Back in the pause menu = resume; Back again in game = pause (never quits)
adb shell input keyevent KEYCODE_BACK; sleep 2
adb shell input keyevent KEYCODE_BACK; sleep 2.5
shot 14_after_back
PID2=$(gamepid)

# Background / foreground keeps state
adb shell input keyevent KEYCODE_HOME; sleep 2
adb shell monkey -p $PKG -c android.intent.category.LAUNCHER 1 >/dev/null 2>&1   # like tapping the icon
sleep 3
shot 15_after_home
PID3=$(gamepid)

adb logcat -d > "$OUT/logcat.txt"
grep -E 'liero-fps|liero-layout|liero:' "$OUT/logcat.txt" | tail -20

status=0
if [ -z "$PID1" ]; then echo "ERROR: game process never started"; status=1; fi
if [ "$PID1" != "$PID2" ]; then echo "ERROR: game process changed/died after Back ($PID1 -> $PID2)"; status=1; fi
if [ "$PID1" != "$PID3" ]; then echo "WARN: game process changed after Home ($PID1 -> $PID3)"; fi
if grep -E "FATAL EXCEPTION|Fatal signal|liero: fatal" "$OUT/logcat.txt" | grep -v "^--"; then echo "ERROR: crash in logcat"; status=1; fi
exit $status

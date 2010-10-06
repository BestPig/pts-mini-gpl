#! /bin/bash --
#
# us.pts_magyar.sh: Script to install the pts_magyar layout for X11 (X.Org)
# by pts@fazekas.hu at Sat Jun 12 22:37:08 CEST 2010
#
# This script hash been tested and found working on Ubuntu Karmic and Ubuntu
# Lucid.

if test "$EUID" = 0; then SUDO=; else SUDO=sudo; fi

exec $SUDO bash /dev/stdin "$@" <<'ENDSUDO'

set -ex

perl -pi -0777 -e \
    's@^// BEGIN pts_magyar\s.*?\n// END pts_magyar[ \t]*\n@@mgs' \
    /usr/share/X11/xkb/symbols/us
cat >>/usr/share/X11/xkb/symbols/us <<'END'
// BEGIN pts_magyar
//
// us.pts_magyar: pts_magyar layout, append to /usr/share/X11/xkb/symbols/us
// by pts@fazekas.hu at Wed Jul 23 21:39:45 CEST 2008
//
// Register on Hardy: sudo bash: echo '  pts_magyar      us: pts magyar' >>/usr/share/X11/xkb/rules/xorg.lst
// Test: setxkbmap 'us(pts_magyar),hu'
// Test: setxkbmap 'us(pts_magyar),hu' -print | xkbcomp - $DISPLAY
//
// Register on Ubuntu Karmic:
// (strace -e open -o key.log gnome-keyboard-properties) reports evdev.lst and evdev.xml.
// $ sudo bash: echo '  pts_magyar      us: pts magyar' >>/usr/share/X11/xkb/rules/evdev.lst
// ?$ sudo bash: echo '  pts_magyar      us: pts magyar' >>/usr/share/X11/xkb/rules/base.lst
// * Add this a bit above <name>euro</name> in /usr/share/X11/xkb/rules/evdev.xml
//        <variant>
//          <configItem>
//            <name>pts_magyar</name>
//            <description>pts magyar</description>
//          </configItem>
//        </variant>
// * ?Ditto for base.xml
// * ?Ditto for xfree86.xml
// * ?Ditto for base.xml

partial alphanumeric_keys modifier_keys
xkb_symbols "pts_magyar" {
    name[Group1]= "USA - pts magyar";

    key <LSGT>  { [ less, greater, iacute, Iacute ] };

    key <TLDE> { [ grave, asciitilde, iacute, Iacute ] };
    key <AE01> { [ 1, exclam ] };
    key <AE02> { [ 2, at ] };
    key <AE03> { [ 3, numbersign ] };
    key <AE04> { [ 4, dollar ] };
    // 0xA4==degree in EuroSign in biki
    key <AE05> { [ 5, percent, EuroSign, EuroSign ] };
    key <AE06> { [ 6, asciicircum ] };
    key <AE07> { [ 7, ampersand ] };
    key <AE08> { [ 8, asterisk ] };
    key <AE09> { [ 9, parenleft ] };
    key <AE10> { [ 0, parenright, odiaeresis, Odiaeresis ] };
    key <AE11> { [ minus, underscore, udiaeresis, Udiaeresis ] };
    key <AE12> { [ equal, plus, oacute, Oacute ] };

    key <AD01> { [ q, Q ] };
    key <AD02> { [ w, W ] };
    key <AD03> { [ e, E ] };
    key <AD04> { [ r, R ] };
    key <AD05> { [ t, T ] };
    key <AD06> { [ y, Y, y, Y, y, Y, y, Y, y, Y, y, Y, y, Y ] };
    key <AD07> { [ u, U ] };
    key <AD08> { [ i, I ] };
    key <AD09> { [ o, O ] };
    key <AD10> { [ p, P ] };
    key <AD11> { [ bracketleft, braceleft, odoubleacute, Odoubleacute ] };
    key <AD12> { [ bracketright, braceright, uacute, Uacute ] };

    key <AC01> { [ a, A ] };
    key <AC02> { [ s, S ] };
    key <AC03> { [ d, D ] };
    key <AC04> { [ f, F ] };
    key <AC05> { [ g, G ] };
    key <AC06> { [ h, H ] };
    key <AC07> { [ j, J ] };
    key <AC08> { [ k, K ] };
    key <AC09> { [ l, L ] };
    key <AC10> { [ semicolon, colon, eacute, Eacute ] };
    key <AC11> { [ apostrophe, quotedbl, aacute, Aacute ] };
    key <BKSL> { [ backslash, bar, udoubleacute, Udoubleacute ] };

    key <AB01> { [ z, Z, z, Z, z, Z, z, Z, z, Z, z, Z, z, Z ] };
    key <AB02> { [ x, X ] };
    key <AB03> { [ c, C ] };
    key <AB04> { [ v, V ] };
    key <AB05> { [ b, B ] };
    key <AB06> { [ n, N ] };
    key <AB07> { [ m, M ] };
    key <AB08> { [ comma, less, division, division ] };
    key <AB09> { [ period, greater, endash, endash ] };
    key <AB10> { [ slash, question ] };

    key <CAPS> { [ Caps_Lock ]	};
    key <KPDL> { [ KP_Delete, KP_Decimal ] };
    include "level3(ralt_switch)"
};
// END pts_magyar
END

# evdev.lst is enough for Ubuntu Lucid.
for F in /usr/share/X11/xkb/rules/{base,evdev,xorg,xfree86}.lst; do
  if test -f "$F"; then
    perl -pi -e '$_="" if /\bpts_magyar\b/' "$F"
    # After /^! variant\n/
    perl -pi -0777 -e 's@^(  dvorak-intl[ \t]*us: .*)@$1\n  pts_magyar us: pts magyar@gm' "$F"
  fi
done

# evdev.xml is enough for Ubuntu Lucid.
for F in /usr/share/X11/xkb/rules/{base,evdev,xorg,xfree86}.xml; do
  if test -f "$F"; then
    perl -pi -0777 -e 's@^[ \t]*<variant>\s*<configItem>\s*<name>pts_magyar</name>.*?</variant>[ \t]*@@gsm' "$F"
    perl -pi -0777 -e 's@^([ \t]*<variant>\s*<configItem>\s*<name>dvorak-intl</name>.*?</variant>)[ \t]*\n@$1\n        <variant>\n          <configItem>\n            <name>pts_magyar</name>\n            <description>pts magyar</description>\n          </configItem>\n        </variant>\n@gsm' "$F"
  fi
done
ENDSUDO

# Z21 emulator @ Arduino Uno for Roco Wlan-Multimaus

This project is heavily based on works on <http://www.pgahtow.de/>:

 * [Z21 - Slave am XpressNet](http://pgahtow.de/wiki/index.php?title=Z21_mobile/en)
 * [XpressNET client library](http://pgahtow.de/wiki/index.php?title=XpressNet)

This project's contribution is just a few fixes of original firmware to make it
work more smoothly with Roco Wlan-Multimaus.

## Background

Z21-LAN protocol is not simple. Is is necessary to hold quite a lot of state
data and Arduino is not capable of doing it (it has not enough memory). Thus
the autor of original firmware did a few simplifications. These simplifications
might work generally, but they do not provide best user experience for users
of Roco Wlan-Multimaus or Roco Z21 App.

This fork tunes these "simplifications" to provide best experience for Z21 App
and Roco Wlan-Mutlimaus specifically.

See commit history for list of changes.

**It is not a purpose of this project to provide fully-working Z21 emulator.
As noted before, this is not possible on Arduino.**

## Praxis

This project was tested on real TT layout of Model Railroader Club in Brno,
Czech Republic using [this hardware](http://www.dccmm.cz/index.php/dcc-modely/jak-na-ovladani-dcc-kolejiste-tabletem-za-pomoci-arduina) (in czech only).
It works so far, known issues are described in <issues.md>.

# dbus-spy

dbus-spy is a debug tool intended for the Venus embedded Linux platform created by
[Victron Energy](http://victronenergy.com).
This is the platform running on the CCGX and venus GX units. This application provides a realtime
view on all properties published to the D-Bus. It is also possible to change the values of those
properties (whenever the application publishing the property allows it).

On startup dbus-spy will present a list of all victron D-Bus services on the system (the one whose
name starts with com.victronenergy. Those services are considered to be compatible with the methods
and signals we need). When activated all properties within the service will be shown together with
the current value. The values will be updated in realtime. It is also possible to change the value.

## Keyboard shortcuts

* left-arrow: open the selected D-Bus service and show the list of properties
* right-arrow: go back to the service list
* up/down-arrow, page up/down: move up/down (yes, really!)
* enter: edit the value of the current property
* t: switch between property values and text
* q: quit the apllication. ctrl-c will do the same job

## Compilation

dbus-spy relies on several external libraries:
* QT-4.8
* libncurses and libform5
* velib This is the Victron Energy platform library. Unfortunately this library is not available
  as open source. Please contact Victron Energy if you need it.

You will also need the
[venus SDK](https://www.victronenergy.com/live/open_source:ccgx:setup_development_environment).
As of this moment you need to use version 1.40.

## Some ceveats

* There is an issue with some of the python services that do not fully conform with the Victron
  standard: they will no supply a dictionary of all values when a GetValue method is called on the
  root element (the localsettings application which provided com.victronenergy.settings is the most
  important one. This makes initialization of the propertylist very slow. If you need to access such
  a service, use the --introspect option on startup.
* The application is written in C++ and uses the QT framework for D-Bus communication. However,
  the GUI is done using ncurses. The reason for this odd combination is that dbus-spy started as
  a QT+QML application running on PC linux. Someday I decided that being able to run the application
  on the embedded device itself would be more useful. So I rewrote the GUI and kept the D-Bus code.
* If you change a value of a property no checks are performed by the application. If the entered
  text strin parses to a floating point value, it will be sent to the D-Bus as a 'double'
  value. Otherwise, it will be parsed an integer. If that does not work, the string itself will be
  sent. The receiving application may expect another type and may refuse to handle the value.
* Changing values on D-Bus services may be dangerous. Only do this if you know what you are doing.
* The screen will not be redrawn when the size of the terminal changes. So chose it wisely before
  starting the application.
* Sometimes parts of the screen are not redrawn properly, especially when changing quickly between
  several services.

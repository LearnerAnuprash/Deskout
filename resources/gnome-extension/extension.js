// Deskout Reading Mode: desaturates everything GNOME Shell draws.
// Deskout toggles it over D-Bus (service org.gnome.Shell,
// object /app/deskout/ReadingMode). The filter is dropped automatically when
// the Deskout process that enabled it disappears, so a crash never leaves
// the screen stuck in grayscale.

import Clutter from 'gi://Clutter';
import Gio from 'gi://Gio';

import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';

const OBJECT_PATH = '/app/deskout/ReadingMode';
const INTERFACE_XML = `
<node>
  <interface name="app.deskout.ReadingMode">
    <method name="SetEnabled">
      <arg type="b" name="enabled" direction="in"/>
    </method>
    <method name="IsEnabled">
      <arg type="b" name="enabled" direction="out"/>
    </method>
    <property name="Version" type="u" access="read"/>
  </interface>
</node>`;

export default class DeskoutReadingModeExtension extends Extension {
    enable() {
        this._effect = null;
        this._callerWatch = 0;
        this._dbus = Gio.DBusExportedObject.wrapJSObject(INTERFACE_XML, this);
        this._dbus.export(Gio.DBus.session, OBJECT_PATH);
    }

    disable() {
        this._setGrayscale(false);
        this._dbus.unexport();
        this._dbus = null;
    }

    get Version() {
        return 1;
    }

    SetEnabledAsync([enabled], invocation) {
        this._setGrayscale(enabled);
        if (enabled)
            this._watchCaller(invocation.get_sender());
        invocation.return_value(null);
    }

    IsEnabled() {
        return this._effect !== null;
    }

    _setGrayscale(enabled) {
        if (enabled && !this._effect) {
            this._effect = new Clutter.DesaturateEffect({factor: 1.0});
            Main.uiGroup.add_effect(this._effect);
        } else if (!enabled && this._effect) {
            Main.uiGroup.remove_effect(this._effect);
            this._effect = null;
        }
        if (!enabled)
            this._unwatchCaller();
    }

    _watchCaller(sender) {
        this._unwatchCaller();
        this._callerWatch = Gio.bus_watch_name(
            Gio.BusType.SESSION, sender, Gio.BusNameWatcherFlags.NONE,
            null, () => this._setGrayscale(false));
    }

    _unwatchCaller() {
        if (this._callerWatch) {
            Gio.bus_unwatch_name(this._callerWatch);
            this._callerWatch = 0;
        }
    }
}

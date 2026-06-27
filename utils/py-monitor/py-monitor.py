import dbus
import dbus.mainloop.glib
import socket
import sys

from gi.repository import GLib
from gi.repository import Gio

cntr = 0

def update():
    global cntr
    cntr += 1
    print(f"Update {cntr}")
    return True


def fabricate_socket(port=20000):
    print(f"listening for UDP traffic on {port}")
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM | socket.SOCK_NONBLOCK, 0)
    s.bind(('', port))
    return s


def handle_data_received(channel, condition, s):
    (data, src) = s.recvfrom(4096)
    print(data.decode('utf-8'), end='', flush=True)
    return True


dbus.mainloop.glib.threads_init()
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
mainloop = GLib.MainLoop()

port = 20000
if len(sys.argv) > 1:
    port = int(sys.argv[1])

s = fabricate_socket(port)
io_channel = GLib.IOChannel.unix_new(s.fileno())
id = GLib.io_add_watch(io_channel, GLib.IOCondition.IN, handle_data_received, s)
#GLib.source_remove(id)
#mainloop.io_add_watch(io_channel)

mainloop.run()
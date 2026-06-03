export PKG_CONFIG_PATH=/path/to/glib-prefix/lib/loongarch64-linux-gnu/pkgconfig:$PKG_CONFIG_PATH

pkg-config --modversion glib-2.0

# Note: Executing this script directly via Bash will spawn a child process to set the GLib version. For better control and visibility, it is recommended to copy the commands and run them manually in the terminal.


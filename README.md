# XFCE Hosts Plugin

This provides an XFCE panel plugin to quickly enable/disable `/etc/hosts` hosts. It's intended
for web development use, where e.g. you want your local web server to point to `www.example.com`
domain to match how it will be when deployed.n n ;/

## Build / Installation

Update `configure.ac` as needed, e.g. to change install paths.

```shell
> sudo apt install xfce4-dev-tools autopoint libxfce4ui-2-dev
> ./autogen.sh
> make
> sudo make install
> xfce4-panel --restart
```

For debugging, you can run do:

```shell
> xfce4-panel -q
> PANEL_DEBUG=1 xfce4-panel
```
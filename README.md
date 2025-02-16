# Development setup

```shell
> sudo apt install xfce4-dev-tools autopoint libxfce4ui-2-dev
> ./autogen.sh
> make
> sudo make install
> xfce4-panel --restart
```

This installs:
- shared library: `/usr/local/lib/xfce4/panel/plugins`
- desktop file: `/usr/local/share/xfce4/panel/plugins`

Apparenntly doesn't like that local location; copying here it will show up:

```shell
> sudo cp /usr/local/lib/xfce4/panel/plugins/libhosts.so /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/
> sudo cp /usr/local/lib/xfce4/panel/plugins/libhosts.la /usr/lib/x86_64-linux-gnu/xfce4/panel/plugins/
> sudo cp /usr/local/share/xfce4/panel/plugins/hosts.desktop /usr/share/xfce4/panel/plugins/
```

Debugging:

```shell
> xfce4-panel -q
> PANEL_DEBUG=1 xfce4-panel
```

# Usage

Create a new user group with permission to modify `/etc/hosts`. This allows the xfce plugin to
modify `/etc/hosts` without you needing to enter a sudo password.

```shell
> sudo setfacl -m $USER:rw /etc/hosts
```

[See this Q/A and the security implications of doing so](https://askubuntu.com/questions/895640/can-i-edit-hosts-without-sudo)
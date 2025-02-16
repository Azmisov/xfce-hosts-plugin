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
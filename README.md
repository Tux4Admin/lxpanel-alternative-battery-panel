# Battery State Plugin for LXPanel
lxpanel-alternative-battery-panel
A battery monitor plugin for LXPanel, which is able to show a battery icon and/or the state of charge in percent

![alt text](https://github.com/Tux4Admin/lxpanel-alternative-battery-panel/blob/master/pics/plugin_1.png)

This is a simple battery plugin for LXPanel (default panel of the LXDE Desktop Enviroment), which is able to show a battery icon
and/or the state of charge in percent. I have tested the plugin with Lubuntu, Arch Linux and Fedora. 

## Compile the source code
If you like to compile the source code, download the code, go to the folder and use this command to compile the plugin.

```
gcc -Wall `pkg-config --cflags gtk+-2.0 lxpanel` -shared -fPIC BatteryStatePlugin.c batt_sys.c -o BatteryStatePlugin.so `pkg-config --libs lxpanel`
```

Make sure, you have installed all the necessary libaries. You can find them on the LXDE wiki (https://wiki.lxde.org/en/How_to_write_plugins_for_LXPanel)


## Use the applet
If you want to use the applet with LXPanel, compile the code or download the plugin file BatteryStatePlugin.so. For using the battery
icon, you need to install the awesome-font.

On Lubuntu, Ubuntu or Debian with lxpanel installed you have to run this commands:
```
sudo apt install fonts-font-awesome
sudo cp BatteryStatePlugin.so /usr/lib/x86-64linux-gnu/lxpanel/plugins/
```


On Arch Linux you can do this, by executing follow command as root:
```
pacman -S ttf-font-awesome
```
After that you can copy the plugin file to /usr/lib/lxpanel/plugins

On Fedora you have to execute this commands as root:
```
dnf install fontawesome-fonts
cp BatteryStatePlugin.so /usr/lib64/lxpanel/plugins
```

## Configure BatteryStatePlugin
After copying the plugin file to lxpanel/plugins, you have to restart the LXPanel. After that you can add the BatteryStatePlugin like
any other applet (right click on the LXPanel, select add/remove Panel items, click add). After that a label with "n.s" will appear on 
the panel. "n.s" stands for nothing selected to be displayed. After a right click on the label you can edit the settings.

![alt text](https://github.com/Tux4Admin/lxpanel-alternative-battery-panel/blob/master/pics/settings.png)

Here you can select what to display. If you want the percentage text to have charging or discharging color set the checkbox. You an choose the charging and discharging color. If the battery reaches a state of charge, which you can set, a notification will be sent. You can customize the text of the notification. At last you can select the number of the battery, you are using.





# Plugin Template

Is this folder you will find a simple plugin example for the signal-composer binding.

The json file defines a plugin defining a signal.
The cpp file implements a function linked to the previous signal.

## Prerequisites

Install the signal-binding and the devel version if you haven't yet:
```bash
dnf install signal-composer-binding signal-composer-binding-devel
```

## Build and Install your plugin

To be able to build your plugin, change ```set(TARGET_NAME generated_plugin)``` in
the CMakeLists.txt with your plugin's name.

At the root of your plugin project (replace "generated-plugin" by the name of
your plugin).

```bash
mkdir build && cd build
cmake ..
make generated-plugin
make install_generated-plugin
```

Now activate the plugin in the can-low-level.
To do so, copy the config json file **control-signal-composer-config.json** in the directory of the actual signal-composer configuration file. This file should be located in /var/local/lib/afm/applications/signal-composer-binding/etc/.

Congratulations, your plugin is now ready to be used.

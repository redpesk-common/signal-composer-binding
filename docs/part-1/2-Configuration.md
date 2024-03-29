# Configuration

Configuration defines all in **Signal composer** each signals and sources has
to be defined in it. At start, configuration will be searched in default
binding configuration directory which should be _/var/local/lib/afm/applications/signal-composer/VER/etc/_,
VER is the version of **signal composer**.
Binding search for a file name as _init-daemon*.json_. Others files could be
used to split sections and don't have 1 big fat definitions file.

Saying that you have 4 sections to define:

- **metadata**: Main parts and the only one that can't be in a separates
 configuration. This must appears in the main configuration file.
- **plugins** (optional): Declare plugins that will be used by *sources* and
 *signals* for the subscription and composition.
- **sources**: Declare **low level** signals sources (eg. low-can, gps, mraa,
 ...).
- **signals**: Declare signals, virtuals and raw.

## Metadata

Here we define some metadata about **signal composer** binding. Fields to configure
are :

- **uid**: self-explanatory
- **version** (optional): self-explanatory
- **api** (optional): name that the binding will be initialized to and later
 be accessible by **Application Framework**
- **info** (optional): self-explanatory
- **require** (optional): list of required external apis.

## Plugins

This section is the only which is optional, it is needed if you develop
specifics C/C++ plugins to be used with signal-composer. LUA and API
consumption does not need plugins.

Default path to search for a plugin is in the binding library directory
in a subdirectory _plugins_ _/var/local/lib/afm/applications/signal-composer/VER/lib/plugins_.
Else you could use the environment variable _CONTROL_PLUGIN_PATH_ with a
semicolon separated list of directory.

Fields are:

- **uid**: Define the plugin name. This is that label that will be used on
 **sources** and **signals** callbacks.
- **ldpath** (optional): path to the plugin directory
- **info** (optional): self-explanatory
- **basename**: shared library file name **without** the extension.
- **files** (optional): list of additionnals files. **ONLY NAME** or part of
 it, without extension. Don't mix up section object with this key, either one
 or the other but avoid using both

## Sources

A source is a **low level** API that will be consume by the **signal composer**
to be able to expose those signals with additionnals treatments, filtering,
thinning,... and create new virtuals signals composed with basic raw signals.

A source is defined with following fields:

- **uid**: An unique identifier name for thatuid source API.
- **api**: Name of the source API.
- **info** (optional): self-explanatory
- **init** (optional): an **action** to take to initialize a source. May you
 have  to call a verb from that API, of create a files etc.
- **getSignals** (optional); an **action** to take to get signals from that
 source. These callback will be used for each signals defined later in the
 **signals** section. Dedicated arguments for each signal could be defined in
 **signals**.
- **files** (optional): list of additionnals files. **ONLY NAME** or part of
 it, without extension. Don't mix up section object with this key, either one
 or the other but avoid using both

## Signals

A signal definition could be either a **raw** one or a **virtual** one. A
 **virtual signal** is a set of existing **raw signals** associated to an
 **action** on reception which will compute the value of the signal.

- **uid**: Unique identifier used inside **signal composer**, used to compose
 virtual signals.
- **event**: specify a **raw signal** coming from **low level** sources.
 Couldn't be used with **depends** field, only one of them is possible.
- **depends**: specify others signals **id** that compose it (eg: heading is
 composed with longitude+latitude signals.). Couldn't be used with **event**
 field at same time.
- **retention** (optional): retention duration in seconds
- **unit** (optional): Unit used to exprime the signal
- **frequency** (optional): Frequency maximum at which the signal could be
 requested or sent. This is a thinning made at **high level** so not best
 suited for performance. Used **low level** native filtering capabilities when
 possible.
- **getSignalsArgs**: a JSON object used at subscription time. Meant to enabled
 filtering capabilities at subscription and to be able to customize in general
 a subcription request by signal if needed.
- **onReceived**: an **action** to take when this signal is received!
- **files** (optional): list of additionnals files. **ONLY NAME** or part of
 it, without extension. Don't mix up section object with this key, either one
 or the other but avoid using both

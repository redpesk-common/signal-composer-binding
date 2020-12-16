# Signal Composer API

## *subscribe/unsubscribe* a signal

Using subscribe you can get update on change for signals you chose and you can
using wildcard to subscribe several signals in the same time.

```json
signal-composer rear_left_door "subscribe"
ON-REPLY 1:signal-composer/rear_left_door: {"jtype":"afb-reply","request":{"status":"success","uuid":"3d4b743b-7ac6-4d3c-8fce-721107f9dee5"}}
```

Then event comes up like the following:

```json
ON-EVENT signal-composer/257b343e-8ea9-4cd7-8f9e-1904fa77f8f2({"event":"signal-composer\/257b343e-8ea9-4cd7-8f9e-1904fa77f8f2","data":{"uid":"rear_left_door","event":"low-can\/vcar\/doors.rear_left.open","timestamp":4833910845032292484,"value":false},"jtype":"afb-event"})
```

Unsubscribe happens the same way. When no more signals are holded by the client
then it unsubscribe from the *AGL Application Framework* event handle.

You can explicitly unsubscribe from events:

```json
signal-composer rear_left_door "unsubscribe"
ON-REPLY 2:signal-composer/rear_left_door: {"jtype":"afb-reply","request":{"status":"success","uuid":"3d4b743b-7ac6-4d3c-8fce-721107f9dee5"}}
```

## *get* signal's value

You can get a signal value be requesting the API with the verb *get*:

```json
signal-composer vehicle_speed "get"
signal-composer vehicle_speed { "get": {"average": 10} }
signal-composer vehicle_speed { "get": {"minimum": 10} }
signal-composer vehicle_speed { "get": {"maximum": 10} }
```

You apply some simple mathematical functions by default present in the
binding, by default **last** is used:

- **average**: make an average on X latest seconds.
- **minimum**: return the minimum value found in the X latest seconds.
- **maximum**: return the maximum value found in the X latest seconds.
- **last**: return the latest value.

## *change* signal properties

Some properties could be changed at runtime for signals. You can change the
following:

* *retention*: seconds of retention to hold signal's value.
* *frequency*: frequency of outputed event of this signal. Could be also passed.
to event's sources as parameter at subscription if supported by the source api.
* *unit*: Unit of the signal.
* *metadata*: Anything you want as a JSON object metadata.

```json
signal-composer vehicle_speed { "change": {"retention": 2} }
ON-REPLY 1:signal-composer/vehicle_speed: {"jtype":"afb-reply", "request":{ "status":"success", "info":"change"}}
signal-composer vehicle_speed { "change": {"frequency": 5} }
ON-REPLY 2:signal-composer/vehicle_speed: {"jtype":"afb-reply", "request":{ "status":"success", "info":"change"}}
signal-composer vehicle_speed { "change": {"unit": "mph"} }
ON-REPLY 3:signal-composer/vehicle_speed: {"jtype":"afb-reply", "request":{ "status":"success", "info":"change"}}
signal-composer vehicle_speed { "change": {"metadata": {"car_model": "Auris", "car_maker": "Toyota"}} }
ON-REPLY 3:signal-composer/vehicle_speed: {"jtype":"afb-reply", "request":{ "status":"success", "info":"change"}}
```

## Get signal's *config*

As well, you can simply retrieve the current configuration of a signal with the
action *config* of the signal's verb:

```json
signal-composer vehicle_speed config
ON-REPLY 1:signal-composer/vehicle_speed: OK
{
  "jtype":"afb-reply",
  "request":{
    "status":"success",
    "info":"config"
  },
  "response":{
    "unit":"mph",
    "metadata": {"car_model": "Auris", "car_maker": "Toyota"},
    "retention":2,
    "frequency": 5
  }
}
```

## General purpose verbs

### addObjects

Let you add sources or signals objects to the signal composer service after
its initialization phase. Use this verb and specify the file as argument, you
could use only the file name or the file name with its absolute path.

```json
signal-composer addObjects {"file": "sig_doors.json"}
ON-REPLY 1:signal-composer/addObjects: {"jtype":"afb-reply","request":{"status":"success","uuid":"00d7a519-816e-486a-8163-3afb1face4fa"}}
signal-composer addObjects {"file": "/tmp/sig_doors.json"}
ON-REPLY 2:signal-composer/addObjects: {"jtype":"afb-reply","request":{"status":"success"}}
```

You can follow the activity using the service log journal and check that the
correct number of objects has been added.

> **CAUTION**: You need to get the following permission to be able to load new
objects : `urn:AGL:permission::platform:composer:addObjects`

### list

Verb **list** will output the list of defined signals.

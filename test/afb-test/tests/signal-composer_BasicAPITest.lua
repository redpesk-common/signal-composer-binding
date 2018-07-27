--[[
   Copyright (C) 2018 "IoT.bzh"
   Author Clément Malléjac <clementmallejac@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.


   NOTE: strict mode: every global variables should be prefixed by '_'
--]]

local testPrefix ="signal_composer_BasicAPITest_";

-- This tests the 'list' verb of the signal-composer API
_AFT.testVerbStatusSuccess(testPrefix.."list","signal-composer","list", {});


-- This tests the 'subscribe' verb of the signal-composer API
_AFT.testVerbStatusSuccess(testPrefix.."subscribe","signal-composer","subscribe",{ signal = "longitude"},nil,
   function()
       _AFT.callVerb("signal-composer","unsubscribe",{ signal = "longitude"})
   end);


-- This tests the 'unsubscribe' verb of the signal-composer API
_AFT.testVerbStatusSuccess(testPrefix.."unsubscribe","signal-composer","unsubscribe",{ signal = "heading"},
   function()
       _AFT.callVerb("signal-composer","subscribe",{ signal = "heading"})
   end, nil);

-- This tests the 'unsubscribe' verb of the signal-composer API when we are not actually subscribed to a signal
_AFT.testVerbStatusSuccess(testPrefix.."doubleUnsubscribe","signal-composer","unsubscribe",{ signal = "latitude"},
   function()
       _AFT.callVerb("signal-composer","unsubscribe",{ signal = "latitude"})
   end,nil);

-- This tests the 'unsubscribe' verb of the signal-composer API when unsubscribing from a non-existing signal
_AFT.testVerbStatusSuccess(testPrefix.."unsubscribeNonExistingSignal","signal-composer","unsubscribe",{ signal = "notASignal"});

-- This tests the 'get' verb of the signal-composer API, without any options, this should return the last value for that signal
_AFT.testVerbStatusSuccess(testPrefix.."getNoFilter","signal-composer","get",{signal= "fuel_level"});

-- This tests the 'get' verb of the signal-composer API, with the 'average' option, this should return the average value over the last X seconds
_AFT.testVerbStatusSuccess(testPrefix.."getFilterAvg","signal-composer","get",{signal= "odometer", options= {average= 10}});

-- This tests the 'get' verb of the signal-composer API, with the 'minimum' option, this should return the minimum value over the last X seconds
_AFT.testVerbStatusSuccess(testPrefix.."getFilterMin","signal-composer","get",{signal= "latitude", options= {minimum= 10}});

-- This tests the 'get' verb of the signal-composer API, with the 'maximum' option, this should return the maximum value over the last X seconds
_AFT.testVerbStatusSuccess(testPrefix.."getFilterMax","signal-composer","get",{signal= "vehicle_speed", options= {maximum= 10}});

--[[ This tests the 'addObjects' verb of the signal-composer API, this is by passing the path of a json containing signals
	 then making a get, a subscribe, and an unsubscribe looking for any misbehaviour from signals added with the verb ]]
_AFT.describe(testPrefix.."addObjectsByFile",function()
    _AFT.assertVerbStatusSuccess("signal-composer","addObjects",{file = _AFT.bindingRootDir.."var/sig_test.json"})
    _AFT.assertVerbStatusSuccess("signal-composer","get",{signal= "vehicle_speedTest1",options= {average=10}});
    _AFT.assertVerbStatusSuccess("signal-composer","subscribe",{ signal = "vehicle_speedTest1"});
	_AFT.assertVerbStatusSuccess("signal-composer","unsubscribe",{ signal = "vehicle_speedTest1"});
end);

_AFT.setAfter(testPrefix.."addObjectsByFile",function()
    _AFT.callVerb("signal-composer","unsubscribe",{ signal = "vehicle_speedTest1"})
end)



-- This tests the 'addObjects' verb of the signal-composer API, this is by passing directly the json object as a lua table
_AFT.testVerbStatusSuccess(testPrefix.."addObjectsDirect","signal-composer","addObjects",
{
	signals=  {
		{
			uid=  "vehicle_speedTest3",
			event=  "txc/vehicle_speed",
			retention=  30,
			unit=  "km/h",
			getSignalsArgs=  {
				event=  "vehicle_speed"
			},
			onReceived=  {
				action=  "lua://convert#_Unit_Converter",
				args=  {
					from=  "km/h",
					to=  "mi/h"
				}
			}
		},
		{
			uid=  "engine_speedTest4",
			event=  "txc/engine_speed",
			retention=  30,
			unit=  "rpm",
			getSignalsArgs=  {
				event=  "engine_speed"
			}
		}
    }
}
);

--[[ This tests the 'addObjects' verb of the signal-composer API, this is by passing directly the json object as a lua table.
	This one has invalid values for most of its field, the binding should not be able to add it ]]
_AFT.testVerbStatusError(testPrefix.."addObjectsDirect_InvalidSignal","signal-composer","addObjects",
{
	signals=  {
		{
			uid=  "invalidSignal",
			event=  "txc/invalidSignal",
			retention=  -1,
			unit=  "invalidSignal",
			getSignalsArgs=  {
				event=  "invalidSignal"
			},
			onReceived=  {
				action=  "lua://convert#_Unit_Converter",
				args=  {
					from=  "km/h",
					to=  "mi/h"
				}
			}
		}
    }
}
);

--[[ This tests the 'addObjects' verb of the signal-composer API, this is by passing directly the json object as a lua table.
	This one is missing the mandatory 'uid' field, the binding should not be able to add it ]]
_AFT.testVerbStatusError(testPrefix.."addObjectsDirect_MissingField","signal-composer","addObjects",
{
	signals=  {
		{
			event=  "txc/invalidSignal2",
			retention=  30,
			unit=  "km/h",
			getSignalsArgs=  {
				event=  "vehicle_speed"
			},
			onReceived=  {
				action=  "lua://convert#_Unit_Converter",
				args=  {
					from=  "km/h",
					to=  "mi/h"
				}
			}
		}
    }
}
);

--[[ This tests the 'addObjects' verb of the signal-composer API, this is by passing the path of a json containing signals
	This one has invalid values for most of its field, the binding should not be able to add it ]]
_AFT.testVerbStatusError(testPrefix.."addObjectsByFile_InvalidSignal","signal-composer","addObjects",{file = _AFT.bindingRootDir.."var/sig_testInvalid.json"});

--[[ This tests the 'addObjects' verb of the signal-composer API, this is by passing the path of a json containing signals
	This one is missing the mandatory 'uid' field, the binding should not be able to add it ]]
_AFT.testVerbStatusError(testPrefix.."addObjectsByFile_Missingfield","signal-composer","addObjects",{file = _AFT.bindingRootDir.."var/sig_incomplete.json"});

-- This tests the 'subscribe' verb of the signal-composer API, with a non existing signal, it should reply with an error
_AFT.testVerbStatusError(testPrefix.."subscribeNonExistingSignal","signal-composer","subscribe",{ signal = "notASignal"});

-- This tests the 'get' verb of the signal-composer API, with an invalid option name, it should reply with an error
_AFT.testVerbStatusError(testPrefix.."getFilterInvalid","signal-composer","get",{signal= "latitude", options= {notValid= 10}});

-- This tests the 'get' verb of the signal-composer API, with a non existing signal, it should reply with an error
_AFT.testVerbStatusError(testPrefix.."getNoFilterInvalidSignal","signal-composer","get",{signal= "notAValidSignal"});

-- This tests the 'get' verb of the signal-composer API, with an invalid option value, it should reply with an error
_AFT.testVerbStatusError(testPrefix.."getFilterOptionInvalidValue","signal-composer","get",{signal= "odometer", options= {average= -1}});

-- This tests the 'get' verb of the signal-composer API, with an invalid parameter name, it should reply with an error
_AFT.testVerbStatusError(testPrefix.."getFilterInvalidFirstArgument","signal-composer","get",{notValidAtAll= "vehicule_speed", options= {average= 10}});

-- This tests the 'get' verb of the signal-composer API, with an invalid second parameter name, it should reply with an error
_AFT.testVerbStatusError(testPrefix.."getFilterInvalidSecondArgument","signal-composer","get",{signal= "vehicule_speed", notValidAtAll= {average= 10}});

_AFT.exitAtEnd();
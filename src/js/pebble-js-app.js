/*global console:true, Pebble:true, window:true */
var config = {};

function ReturnError(errcode) {
	console.warn('PEBBLEJS return error : ' + errcode);
//	Pebble.showSimpleNotificationOnPebble("Futura2.0", 
//										  "Cannot get weather - error "
//										  + errcode);
	Pebble.sendAppMessage({
		"message":"error",
		"error":"HTTP "+errcode
	});
}

function iconFromWeatherId(weatherId,night) {
	var icon = 10;
	switch (weatherId) {
		case 800:
			icon = 0;	//RESOURCE_ID_ICON_CLEAR_DAY
			if (night) {icon = 1;}	//RESOURCE_ID_CLEAR_NIGHT
			break;
		case 803: case 804:
			icon = 8;	//RESOURCE_ID_ICON_PARTLY_CLOUDY_DAY
			if (night) {icon = 9;}	//RESOURCE_ID_PARTLY_CLOUDY_NIGHT
			break;
		case 611: case 612:
			icon = 4;	//RESOURCE_ID_ICON_SLEET
			break;
		case 615:
			icon = 12;	//RESOURCE_ID_ICON_RAIN_SLEET
			break;
		case 616:
			icon = 11;	//RESOURCE_ID_ICON_RAIN_SNOW
			break;
		case 903:
			icon = 14;	//RESOURCE_ID_ICON_COLD
			break;
		case 904:
			icon = 15;	//RESOURCE_ID_ICON_HOT
			break;
		default:
			if (weatherId < 100) {
				icon = 17;		//RESOURCE_ID_ICON_NOT_AVAILABLE
			} else   if (weatherId < 200) {
				icon = 17;		//RESOURCE_ID_ICON_NOT_AVAILABLE
			} else   if (weatherId < 300) {
				icon = 10;		//RESOURCE_ID_ICON_THUNDER
			} else   if (weatherId < 400) {
				icon = 16;		//RESOURCE_ID_ICON_DRIZZLE
			} else   if (weatherId < 500) {
				icon = 10;		//RESOURCE_ID_ICON_THUNDER
			} else   if (weatherId < 600) {
				icon = 2;		//RESOURCE_ID_ICON_RAIN
			} else   if (weatherId < 700) {
				icon = 3;		//RESOURCE_ID_ICON_SNOW
			} else   if (weatherId < 800) {
				icon = 6;		//RESOURCE_ID_ICON_FOG
			} else   if (weatherId < 900) {
				icon = 7;		//RESOURCE_ID_ICON_CLOUDY
				if (night) {icon = 9;}		//RESOURCE_ID_ICON_PARTLY_CLOUDY_NIGHT
			} else   if (weatherId < 1000) {
				icon = 5;		//RESOURCE_ID_ICON_WINDY
			}
			break;
	}
	return icon;
}

function fetchWeather(latitude,longitude) {
	var response;
	var query;
	var url;
	var req;

	query="lat=" + latitude + "&lon=" + longitude;
	url = "http://api.openweathermap.org/data/2.5/weather?" +
    	query +
		"&cnt=1&mode=JSON&appid=c1c0d7485ba71be10eb16d8581c73cff";	

	req = new XMLHttpRequest();
	console.log("PEBBLEJS Opening URL " + url);
	req.open('GET', url , true);

	req.onload = function(e) {
		if (req.readyState == 4) {
			if(req.status == 200) {
				console.log(req.responseText);
				try {
					response = JSON.parse(req.responseText);
				} catch (e) {
					console.error("PEBBLEJS Parsing error:", e); 
					ReturnError(1);
				}
				var temperature, icon, city, sunrise,sunset,night;
				if (response && response.weather && response.weather.length > 0) {
					weather_temperature = Math.round(response.main.temp - 273.15);
					weather_time = response.dt;
					weather_city = response.name;
					weather_sunrise = response.sys.sunrise;
					weather_sunset = response.sys.sunset;
					weather_night = (response.dt < response.sys.sunrise | response.dt > response.sys.sunset);				
					weather_icon = iconFromWeatherId(response.weather[0].id,weather_night);
					
					console.log("ICON: " + weather_icon +
								" TEMP: " + weather_temperature +
								" CITY: " + weather_city +
								" TIME: " + weather_time +
								" NIGHT: " + weather_night
					);

					Pebble.sendAppMessage({
						"message":"weather",
						"time":weather_time,
						"icon":weather_icon,
						"temperature":weather_temperature,
						"city":weather_city},
										  function(e) {
											  console.log("Successfully delivered message with transactionId="
														  + e.data.transactionId);
										  },
										  function(e) {
											  console.log("Unable to deliver message with transactionId="
														  + e.data.transactionId
														  + " Error is: " + e.error.message);
										  });
				} else {
					console.log("PEBBLEJS HTTP Response Error " + response);
					ReturnError(1);
				}
			} else {
				console.log("PEBBLEJS HTTP Error " + req.status);
				ReturnError(req.status);
			}
		} else {
			console.log("PEBBLEJS HTTP readystate error " + req.readystate);
			ReturnError(req.readystate);
		}
	}
	req.send(null);
}

function locationSuccess(pos) {
	var coordinates = pos.coords;
	console.log("PEBBLEJS location success - fetching weather for " + coordinates.latitude + ","+ coordinates.longitude
			   + " accuracy " + coordinates.accuracy + " metres");
//	fetchPlace(coordinates.latitude, coordinates.longitude);
	fetchWeather(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
	console.warn('PEBBLEJS location error (' + err.code + '): ' + err.message);
	Pebble.showSimpleNotificationOnPebble("Futura2.0", 
										  "Cannot get location - error "
										  + err.code + " - " + err.message);
	Pebble.sendAppMessage({
		"message":"error",
		"error":"[" + err.code + "] Location unavailable"
	});
}

//http://dev.w3.org/geo/api/spec-source.html
//maximum location age = 15 mins (90,0000ms)
//
//var locationOptions = { enableHighAccuracy:true, timeout:120000, maximumAge:900000 };
var locationOptions = { enableHighAccuracy:true, timeout:60000, maximumAge:3000 };

Pebble.addEventListener("ready",
						function(e) {
							console.log("PEBBLEJS connected!" + e.ready);
							
							Pebble.sendAppMessage({"message": "ready"});
//							window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
						});

Pebble.addEventListener("appmessage",
						function(location) {
							if (location.payload.message=="update") {
								console.log("PEBBLEJS update message received!");
								window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError, locationOptions);
							}
						});

Pebble.addEventListener("showConfiguration",
						function(configuration) {

    // Load configuration from localStorage
    var json = window.localStorage.getItem('F2options');
    // and if it's there, parse it so we can use it.
	console.log("JSON=" + json);
    if (typeof json === 'string') {
        config = JSON.parse(json);
		console.log("Config LOADED " + JSON.stringify(config));
	} else {
		config = JSON.parse('{"theme_mode":"cyborg","colour_mode":0,"battery_mode":0,"bluetooth_mode":0}');
		console.log("Config CREATED " + JSON.stringify(config));
	}
							
							Pebble.openURL('data:text/html,<!DOCTYPE html><html> \
<head> \
<script src="http://netdna.bootstrapcdn.com/bootstrap/3.0.3/js/bootstrap.min.js"></script> \
<link id="css_theme" href="http://netdna.bootstrapcdn.com/bootswatch/3.0.3/' + config.theme_mode + '/bootstrap.min.css" rel="stylesheet"> \
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no"> \
</head> \
<body> \
<div class="panel panel-default"> \
<h2>Futura2.0</h2> \
  <div class="panel-heading"> \
    <div class="panel-title"><span class="glyphicon glyphicon-cog"></span>Settings</div> \
  </div> \
  <div class="panel-body"> \
<div class="container"> \
    <form role="form" onsubmit="return onSubmit(this)"> \
<div class="form-group"> \
<label for="theme_mode">Theme</label> \
<select id="theme_mode" class="form-control input-lg" onchange="changeCSS(this.value)"> \
        <option>amelia</option> \
        <option>cerulean</option> \
        <option>cosmo</option> \
        <option>cyborg</option> \
        <option>flatly</option> \
        <option>journal</option> \
        <option>readable</option> \
        <option>spacelab</option> \
        <option>simplex</option> \
        <option>slate</option> \
        <option>united</option> \
        <option>yeti</option> \
        </select> \
<label for="colour_mode">Colour scheme</label> \
<select id="colour_mode" class="form-control input-lg" > \
        <option>Dark</option> \
        <option>Light</option> \
        </select> \
<label for="battery_mode">Show battery</label> \
        <select id="battery_mode" class="form-control input-lg" > \
        <option>Do not show battery</option> \
        <option>Only show battery if low</option> \
        <option>Always show battery</option> \
        </select> \
<label for="bluetooth_mode">Show bluetooth</label> \
        <select id="bluetooth_mode" class="form-control input-lg" > \
        <option>Do not show bluetooth</option> \
        <option>Show bluetooth if disconnected</option> \
        <option>Always show bluetooth</option> \
        </select> \
<br> \
        <button type="submit" class="btn btn-primary btn-lg btn-block">Save</button> \
        <button type="cancel" class="btn btn-primary btn-lg btn-block">Cancel</button> \
</div> \
    </form> \
    </div> <!--container--> \
  </div> <!--panel contents--> \
</div> <!--panel--> \
    <script> \
        document.getElementById("theme_mode").value = "' + config.theme_mode + '"; \
        document.getElementById("colour_mode").options.selectedIndex = "' + config.colour_mode + '"; \
        document.getElementById("battery_mode").options.selectedIndex = "' + config.battery_mode + '"; \
        document.getElementById("bluetooth_mode").options.selectedIndex = "' + config.bluetooth_mode + '"; \
        function onSubmit(e) { \
            var result = { \
                theme_mode: document.getElementById("theme_mode").value, \
                colour_mode: document.getElementById("colour_mode").options.selectedIndex, \
                battery_mode: document.getElementById("battery_mode").options.selectedIndex, \
                bluetooth_mode: document.getElementById("bluetooth_mode").options.selectedIndex, \
            }; \
            window.location.href = "pebblejs://close#" + JSON.stringify(result); \
            return false; \
        } \
		function changeCSS(cssTheme) { \
			document.getElementById("css_theme").setAttribute("href", "http://netdna.bootstrapcdn.com/bootswatch/3.0.3/" + cssTheme + "/bootstrap.min.css"); \
		} \
	</script> \
    </body> \
								</html><!--.html');
	}
);

Pebble.addEventListener("webviewclosed",
  function(e) {
    console.log("Configuration window returned: " + e.response);
	      if ((typeof e.response === 'string') && (e.response.length > 0)) {
        // There's a response: assume it's the correct settings from the
        // HTML form, store it in localStorage (necessary?) and push to
        // the Pebble.
        config = JSON.parse(e.response);
    console.log("Configuration stored: " + JSON.stringify(config));
        window.localStorage.setItem('F2options', e.response);
       Pebble.sendAppMessage(e.response);
    }
  }
);



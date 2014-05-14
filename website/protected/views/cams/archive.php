<!DOCTYPE html>
<html>
	<head>
		<meta charset="utf-8">
		<title><?php echo Yii::t('cams', 'View {cam_name}', array('{cam_name}' => $cam->name)); ?></title>
		<style>
			body {
				margin:0px;
				padding:0px;
				overflow:hidden;
			}
			#tbl {
				width:100%;
				height:100%;
			}
			#flash_player {
				width:100%;
				height:100%;
				background-color:black;
			}
			#controls_right {
				width:176px;
				height:100%;
				background-color:black;
				vertical-align:bottom;
			}
			#controls_bottom {
				width:100%;
				height:100px;
				background-color:black;
			}
			div.ui-datepicker {
				font-size:10px;
			}
			.w100 {
				width:100%;
				text-align: center;
			}
			#timepicker div{
				width:100%;
			}
			.ui_tpicker_hour {
				margin:0px 10px 0px 10px;
			}
			.ui_tpicker_minute {
				margin:0px 10px 0px 10px;
			}
			.ui_tpicker_second {
				margin:0px 10px 0px 10px;
			}
		</style>
		<link href="<?php echo Yii::app()->request->baseUrl; ?>/css/jquery-ui-1.10.4.custom.min.css" rel="stylesheet">
		<script src="<?php echo Yii::app()->request->baseUrl; ?>/js/jquery-1.10.2.js"></script>
		<script src="<?php echo Yii::app()->request->baseUrl; ?>/js/jquery-ui-1.10.4.custom.min.js"></script>
		<script src="<?php echo Yii::app()->request->baseUrl; ?>/js/jquery-ui-timepicker-addon.js"></script>
<!--
		<script src="<?php echo Yii::app()->request->baseUrl; ?>/player/url.js"></script>
-->
		<script>
			var recorded_intervals = {};
			var correct_timezone = false;
			var time_update_interval = false;
			var cur_play_time = 0;
			var timepicker_mouse_down = false;

			function setAndMoveTime() {
				if (!timepicker_mouse_down) $("#timepicker").timepicker('setDate', new Date(cur_play_time*1000));
				cur_play_time++;
//				console.log(cur_play_time);
			}

			function setDateTime(unixtime) {
				d = new Date(unixtime*1000);
				$("#date").val((d.getDay()<=9?("0"+d.getDay()):(d.getDay()))+"/"
					+(d.getMonth()<=9?("0"+d.getMonth()):(d.getMonth()))+"/"
					+((d.getYear()<100)?(d.getYear()):(d.getYear()%100))
				);

//				console.log((d.getDay()<=9?("0"+d.getDay()):(d.getDay()))+"/"
//					+(d.getMonth()<=9?("0"+d.getMonth()):(d.getMonth()))+"/"
//					+(d.getYear()<=9?("0"+d.getYear()):(d.getYear())));
			}

			function flashInitialized() {
				flash_initialized = true;

				cur_play_time = parseInt((new Date).getTime()/1000);
				if (time_update_interval)
					clearInterval(time_update_interval);
//				time_update_interval = setInterval(setAndMoveTime, 1000);

				document["MyPlayer"].setSource('<?php echo $uri; ?>', '<?php echo $cam->getSessionId($full); ?>');
//				console.log("LOW  document['MyPlayer'].setSource('<?php echo $uri; ?>', '<?php echo $cam->getSessionId(true); ?>');");
//				console.log("HIGH document['MyPlayer'].setSource('<?php echo $uri; ?>', '<?php echo $cam->getSessionId(); ?>');");
				recorded_intervals = <?php echo $recorded_intervals; ?>;
				recorded_intervals = recorded_intervals.timings
				if (recorded_intervals === undefined) recorded_intervals = [];
				setDateTime((new Date).getTime()/1000);
//				$("#datepicker").datepicker('setDate', new Date(cur_play_time*1000));
//				$("#timepicker").datepicker('setDate', new Date(cur_play_time*1000));
//				console.log("recorded intervals:");
//				console.log(recorded_intervals);

				$("#datepicker").datepicker({
					dateFormat: "dd/mm/y",
					altField: "#date",
					firstDay: 1,
//					showButtonPanel: true,
					changeMonth: true,
					changeYear: true,
					beforeShowDay: function(date) {
						var ret = false;
						$.each(recorded_intervals, function(k, v) {
							s = new Date(v.start*1000); s.setHours(0,0,0,0);
							e = new Date(v.end*1000); e.setHours(23,59,59,999);
							c = new Date().getTime()/1000;
							if ((s <= date) && (date <= e)) {
								ret = true;
							}
						});
						return [(ret), ''];
					}
				});
				$("#datepicker").change(function(){
//					console.log("=======datepicker change start");
					var date = $("#datepicker").datepicker("getDate");
					var start_time = 0;
					// we get only date, so we need to find time for this date
					for (k = 0; k < recorded_intervals.length; k++) {
						v = recorded_intervals[k];
						s = new Date(v.start*1000); s.setHours(0,0,0,0);
						e = new Date(v.end*1000); e.setHours(23,59,59,999);
						if ((s <= date) && (date <= e)) {
							if (s.getTime() == date.getTime()) {
								// if we have interval those start on this date we set time to start time of this interval
								start_time = v.start;
							} else {
								// overwise set time to 00:00:00
								start_time = parseInt(s.getTime()/1000);
							}
							start_time++;
							$("#timepicker").timepicker('setDate', new Date(start_time*1000));
							cur_play_time = start_time;
							if (time_update_interval)
								clearInterval(time_update_interval);
//							time_update_interval = setInterval(setAndMoveTime, 1000);
//							document['MyPlayer'].setSourceSeeked('<?php echo $uri; ?>/nvr/<?php echo $cam->getSessionId($full); ?>', '<?php echo $cam->getSessionId($full); ?>', start_time, start_time+60*60*24);
//							console.log("datepicker time = "+start_time+(new Date(start_time*1000)));
//							console.log("=======datepicker change stop");
							return true;
						}
					}

				});
				$("#timepicker").timepicker({
					timeFormat: 'HH:mm:ss',
					hourGrid: 1,
					minuteGrid: 1,
					secondGrid: 1,
					altField: "#time",
					showTime: false,
					showButtonPanel: false
				});
				$("#timepicker").change(function(){
//					console.log("=======timepicker change start");
//					var start_time = new Date().getTime();
					var date = $("#datepicker").datepicker("getDate");
//					console.log($("#timepicker").datepicker("getDate").getTime()/1000);
					$(".ui_tpicker_hour td").css("color", "");
					$(".ui_tpicker_minute td").css("color", "");
					$(".ui_tpicker_second td").css("color", "");
					$(".rec_div").remove();

					var hh = $("#timepicker").datepicker("getDate").getHours();
					var mm = $("#timepicker").datepicker("getDate").getMinutes();
					var ss = $("#timepicker").datepicker("getDate").getSeconds();
					$.each(recorded_intervals, function(k, v) {
						s = new Date(v.start*1000); s.setHours(0,0,0,0);
						e = new Date(v.end*1000); e.setHours(23,59,59,999);
						if ((s <= date) && (date <= e)) {
							// if this rec interval include current day
							for (h=0; h<=23; h++) {
								var tmp = $("#datepicker").datepicker("getDate"); tmp.setHours(h,0,0,0);
								s2 = parseInt(tmp.getTime()/1000);
								e2 = s2+60*60-1;
								if (s2 < v.end && v.start < e2) {
									$(".ui_tpicker_hour td:contains("+(h<=9?('0'+h):h)+")").css("color", "#00FF00");
									rec_div = "<div class='ui-slider-range ui-widget-header rec_div' style='width:4.166666666666667%;left:"+(4.166666666666667*h)+"%;background:#00FF00;'></div>";
									$(".ui_tpicker_hour_slider .ui-slider-handle").before(rec_div);
									if (h == hh) {
										for (m = 0; m < 60; m++) {
											var tmp2 = $("#datepicker").datepicker("getDate");; tmp2.setHours(h,m,0,0);
											s3 = parseInt(tmp2.getTime()/1000);
											e3 = s3+60-1;
											if (s3 < v.end && v.start < e3) {
												$(".ui_tpicker_minute td:contains("+(m<=9?('0'+m):m)+")").css("color", "#00FF00");
												rec_div = "<div class='ui-slider-range ui-widget-header rec_div' style='width:1.666666666666667%;left:"+(1.666666666666667*m)+"%;background: #00FF00;'></div>";
												$(".ui_tpicker_minute_slider .ui-slider-handle").before(rec_div);
												if (m == mm) {
													for (s = 0; s < 60; s++) {
														var tmp3 = $("#datepicker").datepicker("getDate");; tmp3.setHours(h,m,s,0);
														s4 = parseInt(tmp3.getTime()/1000);
														e4 = s4+60-1;
														if (s4 < v.end && v.start < e4) {
															$(".ui_tpicker_second td:contains("+(s<=9?('0'+s):s)+")").css("color", "#00FF00");
															rec_div = "<div class='ui-slider-range ui-widget-header rec_div' style='width:1.666666666666667%;left:"+(1.666666666666667*s)+"%;background: #00FF00;'></div>";
															$(".ui_tpicker_second_slider .ui-slider-handle").before(rec_div);
														}
													}
												}
											}
										}
									}
								}
							}
						}
					});
//					var end_time = new Date().getTime();
//					console.log('Execution time: ' + (end_time - start_time));
					var tmp_date = $("#datepicker").datepicker("getDate");
					tmp_date.setHours($("#timepicker").datepicker("getDate").getHours(), $("#timepicker").datepicker("getDate").getMinutes(), $("#timepicker").datepicker("getDate").getSeconds());
					start_time = parseInt(tmp_date.getTime()/1000);
					document['MyPlayer'].setSourceSeeked('<?php echo $uri; ?>/nvr/<?php echo $cam->getSessionId($full); ?>', '<?php echo $cam->getSessionId($full); ?>', start_time, start_time+60*60*24);
//					console.log("timepicker time = "+start_time+"  "+(new Date(start_time*1000)));
//					console.log("=======timepicker change stop");
				})
				$("#timepicker").on("mousedown", function(){
					timepicker_mouse_down = true;
				});
				$("#timepicker").on("mouseup", function(){
					timepicker_mouse_down = false;
				});
				$("#live_button").click(function(){
					$("#timepicker").timepicker('setDate', new Date());
					document['MyPlayer'].setSource('<?php echo $uri; ?>/live/<?php echo $cam->getSessionId($full); ?>', '<?php echo $cam->getSessionId($full); ?>');
//					console.log("live");
				});
				$("#download_button").click(function(){
					var tmp_date = $("#datepicker").datepicker("getDate");
					tmp_date.setHours($("#timepicker").datepicker("getDate").getHours(), $("#timepicker").datepicker("getDate").getMinutes(), $("#timepicker").datepicker("getDate").getSeconds());
					start_time = parseInt(tmp_date.getTime()/1000);
					window.open("<?php echo $down.$cam->getRealId($cam->getSessionId($full)); ?>"+"&start="+start_time+"&end="+(start_time+parseInt($("#download_duration").val())));
				});
				$("#timepicker").timepicker('setDate', new Date());
				document['MyPlayer'].setSource('<?php echo $uri; ?>/live/<?php echo $cam->getSessionId($full); ?>', '<?php echo $cam->getSessionId($full); ?>');
//				$("#timepicker").timepicker('setDate', new Date(<?php echo $unixtime; ?>*1000));
			}

//			flashInitialized();

			$(window).on('load resize',function(){
				height = $(window).height() - $("#controls_bottom").height();
				width = $(window).width() - $("#controls_right").width();
				$("#flash_player").css("height", height);
				$("#controls_right").css("height", height);
				$("#flash_player").css("width", width);
			});
		</script>
	</head>
	<body>

	<table id="tbl" border:="0px" cellpadding="0px" cellspacing="0px">
		<tr>
		<td id="flash_player">
			<object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" id="MyPlayer" align="middle">
				<param name="movie" value="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer.swf"/>
				<param name="allowScriptAccess" value="always"/>
				<param name="quality" value="high"/>
				<param name="scale" value="noscale"/>
				<param name="salign" value="lt"/>
				<param name="wmode" value="opaque"/>
				<param name="bgcolor" value="#000000"/>
				<param name="allowFullScreen" value="true"/>
				<param name="FlashVars" value="autoplay=0&playlist=1&buffer=0.3&volume=<?php echo (int)$this->showArchive; ?>.0"/>
				<embed FlashVars="autoplay=0&playlist=1&buffer=0.3&volume=<?php echo (int)$this->showArchive; ?>.0"
					src="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer.swf"
					bgcolor="#000000"
					width="100%"
					height="100%"
					name="MyPlayer"
					quality="high"
					wmode="opaque"
					align="middle"
					scale="showall"
					allowFullScreen="true"
					allowScriptAccess="always"
					type="application/x-shockwave-flash"
					pluginspage="http://www.macromedia.com/go/getflashplayer"
				/>
			</object>
		</td>
		<td id='controls_right'>
			<input type="text" id="download_duration" value="300" size="1"></input><span style="color:grey;">second</span>
			<input type="button" id="download_button" value="Download"></input><br>
			<input type="button" id="live_button" value="Live"></input>
			<input class="w100" type="text" id="date" disabled="true">
			<div id="datepicker"></div>
			<input class="w100" type="text" id="time" disabled="true">
		</td>
		</tr>
		<tr>
		<td colspan=2  id="controls_bottom">
			<div id="timepicker"></div>
		</td></tr>
	</table>
	</body>
</html>
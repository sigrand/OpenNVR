<?php
if ($fullscreen == "true")
	echo '<div class="col-sm-12" style="top:52px;bottom: 0px;left: 0px;width:100%;">';
else
	echo '<div class="col-sm-12" style="padding:0px; position: absolute;top:52px;bottom: 0px;left: 0px;width:100%;">';
?>
<?php
?>
		<div class="panel-body">
		<link href="<?php echo Yii::app()->request->baseUrl; ?>/css/jquery-ui-1.10.4.custom.min.css" rel="stylesheet">
		<script src="<?php echo Yii::app()->request->baseUrl; ?>/js/jquery-1.10.2.js"></script>
		<script src="<?php echo Yii::app()->request->baseUrl; ?>/js/jquery-ui-1.10.4.custom.min.js"></script>
		<script src="<?php echo Yii::app()->request->baseUrl; ?>/js/jquery-ui-timepicker-addon.js"></script>
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
			#flash_players {
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
	<table id="tbl" border:="0px" cellpadding="0px" cellspacing="0px">
		<tr>
		<td id="flash_players">
<?php
	// max 16 cams on screen
	$colors = array(
		1 => "#FF0000",
		2 => "#00FF00",
		3 => "#0000FF",
		4 => "#000000",
		5 => "#FFFF00",
		6 => "#FF00FF",
		7 => "#00FFFF",
		8 => "#080000",
		9 => "#000800",
		10 => "#000008",
		11 => "#800000",
		12 => "#008000",
		13 => "#000080",
		14 => "#008080",
		15 => "#800080",
		16 => "#808000",
	);
	$cams_str = "";
	$recorded_intervals = "[";
	Yii::import('ext.moment.index', 1);
	for ($i=1; $i <= 16; $i++) {
		eval("\$id=\$model->cam${i}_id;");
		eval("\$x=\$model->cam${i}_x;");
		eval("\$y=\$model->cam${i}_y;");
		eval("\$w=\$model->cam${i}_w;");
		eval("\$h=\$model->cam${i}_h;");
		eval("\$descr=\$model->cam${i}_descr;");
		if ($id && ($w > 0) && ($h > 0)) {
			syslog(LOG_ALERT, $id." ".$w." ".$h." i=".$i);
			$cam = Cams::model()->findByPK($id);
			$stream = $cam->getSessionId();
			$stream_low = $cam->getSessionId(true);
			$momentManager = new momentManager($cam->server_id);
			$recorded_intervals .= $momentManager->existence($cam->id).",";
			if(isset($servers[$cam->server_id])) {
				$server = $servers[$cam->server_id];
			} else {
				$server = $servers[$cam->server_id] = Servers::model()->findByPK($cam->server_id);
			}
			$cams_str .= 'cam['.$i.'] = {"url":"rtmp://'.$server->ip.':'.$server->l_port.'/", "stream_low":"'.$stream_low.'", "stream_high":"'.$stream.'"};'."\n";
			echo '<div class="cams" id="player'.$i.'" mw="'.$w.'" mh="'.$h.'" mx="'.$x.'" my="'.$y.'" href="'.$id.'" style="background-color:'.$colors[$i].';';
			echo ' position:absolute;width:'.$w.'%;height:'.$h.'%;left:'.$x.'%;top:'.$y.'%;">';
?>
				<object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" id="MyPlayer<?php echo "$i\""; ?>
				cam_id=<?php echo "$id"; ?>
				align="middle">
				<param name="movie" value="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer_hi_lo.swf"/>
				<param name="allowScriptAccess" value="always"/>
				<param name="quality" value="high"/>
				<param name="scale" value="noscale"/>
				<param name="salign" value="lt"/>
				<param name="wmode" value="opaque"/>
				<param name="bgcolor" value="#000000"/>
				<param name="allowFullScreen" value="true"/>
				<param name="FlashVars" value="autoplay=0&playlist=1&buffer=0.3"/>
				<embed FlashVars="autoplay=0&playlist=1&buffer=0.3&auto_horizontal_mode=true&show_buttons=true"
				src="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer_hi_lo.swf"
				cam_id=<?php echo "$id"; ?>
				bgcolor="#000000"
				width="100%"
				height="100%"
				name="MyPlayer<?php echo "$i\""; ?>
				quality="high"
				wmode="opaque"
				align="middle"
				scale="showall"
				allowFullScreen="true"
				allowScriptAccess="always"
				type="application/x-shockwave-flash"
				pluginspage="http://www.macromedia.com/go/getflashplayer"
				/>
				<?php echo CHtml::link("<img src='".Yii::app()->request->baseUrl."/images/archive.png' style='width:50px;height:50px;position:absolute;top:10px;left:10px;z-index:1'></img>", $this->createUrl('/cams/archive/full/1/id/'.$stream), array("id" => "archive_button_".$i, 'target' => '_blank')); ?>
				</object>
				<?php
				echo '</div>';
			}
		} // for end
		$recorded_intervals .= "{}]";
		echo '<script type="text/javascript">var recorded_intervals='.$recorded_intervals.';</script>';
//exit(0);
		?>
	</div>
</div>
		</td>
		<td id='controls_right'>
			<input type="button" class='w100' id="live_button" value="<?php echo Yii::t('cams', 'Live'); ?>"></input>
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
<script type="text/javascript">
	var correct_timezone = false;
	var time_update_interval = false;
	var cur_play_time = 0;
	var prev_play_time = 0;
	var timepicker_mouse_down = false;
	var hide_buttons_timer = [];
	var cam = new Array();
	var update_players = false;
	<?php echo $cams_str; ?>

	rec_int = new Array();
	$.each(recorded_intervals, function(index, value) {
		if (value != undefined)
		rec_int = rec_int.concat(value.timings);
	});
	recorded_intervals = rec_int;

	function setAndMoveTime() {
		if (prev_play_time+1 != cur_play_time) {
			update_players = true;
			console.log("update_players");
		}
		if (!timepicker_mouse_down) $("#timepicker").timepicker('setDate', new Date(cur_play_time*1000));
		prev_play_time = cur_play_time;
		cur_play_time++;
//		console.log(cur_play_time);
	}
	function setDateTime(unixtime) {
		d = new Date(unixtime*1000);
		$("#date").val((d.getDay()<=9?("0"+d.getDay()):(d.getDay()))+"/"
			+(d.getMonth()<=9?("0"+d.getMonth()):(d.getMonth()))+"/"
			+((d.getYear()<100)?(d.getYear()):(d.getYear()%100))
		);
//		console.log((d.getDay()<=9?("0"+d.getDay()):(d.getDay()))+"/"
//			+(d.getMonth()<=9?("0"+d.getMonth()):(d.getMonth()))+"/"
//			+(d.getYear()<=9?("0"+d.getYear()):(d.getYear())));
	}
	function hidebutton(i) {
		$("#archive_button_"+i).hide();
	}

	function flashInitialized() {
		for (i=1; i<=16; i++) {
			if (cam[i] != undefined) {
				document['MyPlayer'+i].setSource(cam[i].url+'live/', cam[i].stream_low, cam[i].stream_high);
				console.log("document['MyPlayer'+i].setSource(cam[i].url+'live/', cam[i].stream_low, cam[i].stream_high);");
			}
			$("#player"+i).mousemove(function() {
				id = this.id.substring(6);
				$("#archive_button_"+id).show();
				if (hide_buttons_timer[id]) {
					clearInterval(hide_buttons_timer[id]);
					hide_buttons_timer[id] = setInterval("hidebutton("+id+");", 5000);
				}
			});
			hide_buttons_timer[i] = setInterval ("hidebutton("+i+");", 5000);
		}
		setDateTime((new Date).getTime()/1000);
		cur_play_time = parseInt((new Date).getTime()/1000);
		if (time_update_interval)
			clearInterval(time_update_interval);
//		time_update_interval = setInterval(setAndMoveTime, 1000);
	}
	$(window).on('load resize',function() {
		height = $(window).height() - $("#controls_bottom").height();
		width = $(window).width() - 176;
		$("#flash_players").css("height", height);
		$("#controls_right").css("height", height);
		$("#flash_players").css("width", width);
		$("#content").children().css('padding', '0px');
		$("#content").children().children().css('padding', '0px');

		fpw = parseInt($("#flash_players").width());
		fph = parseInt($("#flash_players").height());
		for (i=1; i<=16; i++) {
			$("#player"+i).css('width', fpw * $("#player"+i).attr('mw')/100);
			$("#player"+i).css('height', fph * $("#player"+i).attr('mh')/100);
			$("#player"+i).css('left', fpw * $("#player"+i).attr('mx')/100);
			$("#player"+i).css('top', fph * $("#player"+i).attr('my')/100);
			$("#player"+i).attr('mx');
		}
	});

	$(function() {
				$("#datepicker").datepicker({
					dateFormat: "dd/mm/y",
					altField: "#date",
					firstDay: 1,
					changeMonth: true,
					changeYear: true,
					beforeShowDay: function(date) {
						var ret = false;
						$.each(recorded_intervals, function(k, v) {
							if (v == undefined) return true;
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
//							cur_play_time = parseInt(start_time/1000);
//							if (time_update_interval)
//								clearInterval(time_update_interval);
//							cur_play_time = start_time;
							for (i=1; i<=16; i++) {
								if ((cam[i] != undefined) && (document['MyPlayer'+i].setSource != undefined)) {
									document['MyPlayer'+i].setSource(cam[i].url+'nvr/'+cam[i].stream_low, cam[i].stream_low+'?start='+start_time, cam[i].stream_high+'?start='+start_time);
									console.log("document['MyPlayer'+i].setSource(cam[i].url+'nvr/'+cam[i].stream_low, cam[i].stream_low+'?start='+start_time, cam[i].stream_high+'?start='+start_time);");
								}
							}
//							update_players = false;
//							time_update_interval = setInterval(setAndMoveTime, 1000);
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
					$(".ui_tpicker_hour td").css("color", "#fff");
					$(".ui_tpicker_minute td").css("color", "#fff");
					$(".ui_tpicker_second td").css("color", "#fff");
					$(".rec_div").remove();

					var hh = $("#timepicker").datepicker("getDate").getHours();
					var mm = $("#timepicker").datepicker("getDate").getMinutes();
					var ss = $("#timepicker").datepicker("getDate").getSeconds();
					$.each(recorded_intervals, function(k, v) {
						if (v == undefined) return true;
						s = new Date(v.start*1000); s.setHours(0,0,0,0);
						e = new Date(v.end*1000); e.setHours(23,59,59,999);
						if ((s <= date) && (date <= e)) {
							// if this rec interval include current day
							for (h=0; h<=23; h++) {
								var tmp = $("#datepicker").datepicker("getDate"); tmp.setHours(h,0,0,0);
								s2 = parseInt(tmp.getTime()/1000);
								e2 = s2+60*60-1;
								if (s2 < v.end && v.start < e2) {
									$(".ui_tpicker_hour td:contains("+(h<=9?('0'+h):h)+")").css("color", "white");
									rec_div = "<div class='ui-slider-range ui-widget-header rec_div' style='width:4.166666666666667%;left:"+(4.166666666666667*h)+"%;background:white;'></div>";
									$(".ui_tpicker_hour_slider .ui-slider-handle").before(rec_div);
									if (h == hh) {
										for (m = 0; m < 60; m++) {
											var tmp2 = $("#datepicker").datepicker("getDate");; tmp2.setHours(h,m,0,0);
											s3 = parseInt(tmp2.getTime()/1000);
											e3 = s3+60-1;
											if (s3 < v.end && v.start < e3) {
												$(".ui_tpicker_minute td:contains("+(m<=9?('0'+m):m)+")").css("color", "blue");
												rec_div = "<div class='ui-slider-range ui-widget-header rec_div' style='width:1.666666666666667%;left:"+(1.666666666666667*m)+"%;background:blue;'></div>";
												$(".ui_tpicker_minute_slider .ui-slider-handle").before(rec_div);
												if (m == mm) {
													for (s = 0; s < 60; s++) {
														var tmp3 = $("#datepicker").datepicker("getDate");; tmp3.setHours(h,m,s,0);
														s4 = parseInt(tmp3.getTime()/1000);
														e4 = s4+60-1;
														if (s4 < v.end && v.start < e4) {
															$(".ui_tpicker_second td:contains("+(s<=9?('0'+s):s)+")").css("color", "red");
															rec_div = "<div class='ui-slider-range ui-widget-header rec_div' style='width:1.666666666666667%;left:"+(1.666666666666667*s)+"%;background:red;'></div>";
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
					if (time_update_interval)
						clearInterval(time_update_interval);
					cur_play_time = start_time;
//					if (update_players) {
						for (i=1; i<=16; i++) {
							if ((cam[i] != undefined) && (document['MyPlayer'+i].setSource != undefined)) {
								document['MyPlayer'+i].setSource(cam[i].url+'nvr/'+cam[i].stream_low, cam[i].stream_low+'?start='+start_time, cam[i].stream_high+'?start='+start_time);
								console.log("document['MyPlayer'+i].setSource(cam[i].url+'nvr/'+cam[i].stream_low, cam[i].stream_low+'?start='+start_time, cam[i].stream_high+'?start='+start_time);");
							}
						}
//					}
//					update_players = false;
//					time_update_interval = setInterval(setAndMoveTime, 1000);
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
//					for (i=1; i<=16; i++) {
//						if (cam[i] != undefined) {
//							document['MyPlayer'+i].setSource(cam[i].url+'live/', cam[i].stream_low, cam[i].stream_high);
//						}
//					}
//					console.log("live");
				});
				$("#timepicker").timepicker('setDate', new Date());
				$(window).resize();
	});
</script>
<?php
?>

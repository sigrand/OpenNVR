<?php
if ($fullscreen == "true")
	echo '<div class="col-sm-12" style="top:52px;bottom: 0px;left: 0px;width:100%;">';
else
	echo '<div class="col-sm-12" style="position: absolute;top:52px;bottom: 0px;left: 0px;width:100%;">';
?>
<!--	<div class="panel panel-default" style="width:100%;">-->
<?php
?>
		<div class="panel-body" style="width:100%;">
			<div style="width:100%;">
<script src="<?php echo Yii::app()->request->baseUrl; ?>/player/js/jquery-1.10.2.js"></script>
<script src="<?php echo Yii::app()->request->baseUrl; ?>/player/js/flowplayer-3.2.13.min.js"></script>

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
	$players = "";
	for ($i=1; $i <= 16; $i++) {
		eval("\$id=\$model->cam${i}_id;");
		eval("\$x=\$model->cam${i}_x;");
		eval("\$y=\$model->cam${i}_y;");
		eval("\$w=\$model->cam${i}_w;");
		eval("\$h=\$model->cam${i}_h;");
		eval("\$descr=\$model->cam${i}_descr;");
		if (($w > 0) && ($h > 0)) {
			$cam = Cams::model()->findByPK($id);
			$stream = $cam->getSessionId();
			$stream_low = $cam->getSessionId(true);
			if(isset($servers[$cam->server_id])) {
			$server = $servers[$cam->server_id];
			} else {
			$server = $servers[$cam->server_id] = Servers::model()->findByPK($cam->server_id);
			}
			$players .= 'document["MyPlayer'.$i.'"].setSource("rtmp://'.$server->ip.':'.$server->l_port.'/live/", "'.$stream_low.'", "'.$stream.'");'."\n";

			echo '<div class="cams" id="player'.$i.'" href="'.$id.'" style="background-color:'.$colors[$i].';';
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
				<?php echo CHtml::link("<img src='".Yii::app()->request->baseUrl."/images/archive.png' style='width:50px;height:50px;position:absolute;top:10px;left:10px;z-index:1'></img>", $this->createUrl('/cams/fullscreen/full/1/id/'.$stream), array("id" => "archive_button_".$i, 'target' => '_blank')); ?>
				</object>

				<?php
				echo '</div>';
			}
		}

		?>
		<!--		</div>-->
	</div>
</div>
</div>

<script type="text/javascript">

	var hide_buttons_timer = [];
	function hidebutton(i) {
		$("#archive_button_"+i).hide();
	}
	function flashInitialized() {
		<?php echo $players; ?>
		for (i=1; i<=16; i++) {
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
	}

</script>

</script>

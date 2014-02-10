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
<script type="text/javascript">
	$(function(){
	for (i=1; i<=16; i++) {

	flowplayer("player"+i, <?php echo "\"".Yii::app()->request->baseUrl.'/player/flowplayer-3.2.18.swf'."\""; ?>, {
		wmode: 'opaque',
		clip: {
			provider: 'rtmp',
			bufferLength: 1,
			live : true
		},
		// streaming plugins are configured under the plugins node
		plugins: {
			// here is our rtpm plugin configuration
			rtmp: {
				url: <?php echo "\"".Yii::app()->request->baseUrl.'/player/flowplayer.rtmp-3.2.13.swf'."\""; ?>,
				// netConnectionUrl defines where the streams are found
				netConnectionUrl: <?php echo '"rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/"'; ?>
			},
			controls: null
		}
	});
	}

	x});
</script>

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
	for ($i=1; $i <= 16; $i++) {
		eval("\$id=\$model->cam${i}_id;");
		eval("\$x=\$model->cam${i}_x;");
		eval("\$y=\$model->cam${i}_y;");
		eval("\$w=\$model->cam${i}_w;");
		eval("\$h=\$model->cam${i}_h;");
		eval("\$descr=\$model->cam${i}_descr;");
		if (($w > 0) && ($h > 0)) {
			echo '<div class="cams" id="player'.$i.'" href="'.$id.'" style="background-color:'.$colors[$i].';';
			echo ' position:absolute;width:'.$w.'%;height:'.$h.'%;left:'.$x.'%;top:'.$y.'%;">';
			echo '</div>';
		}
	}

?>
<!--		</div>-->
		</div>
	</div>
</div>

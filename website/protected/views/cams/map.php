<?php
/* @var $this CamsController */
?>
<link rel="stylesheet" href="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.css" />
<script src="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.js"></script>


<div style="display:none;">
<div id="MyPlayer_div" style="width:300px;">
                <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" id="MyPlayer"
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
                <embed FlashVars="autoplay=0&playlist=1&buffer=0.3"
                src="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer_hi_lo.swf"
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
</div>
</div>
<div class="col-sm-12">
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">Камеры</h3>
			</div>
			<div class="panel-body" id="map" style="width:100%;height:400px;">
			</div>
		</div>
	</div>
	<script>

	var cam_id = "";
	function flashInitialized() {
		document["MyPlayer"].setSource(<?php echo '"rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/"'; ?>, cam_id+"_low", cam_id);
	}


	$(document).ready(function(){
		var map = L.map('map');
		L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
			attribution: '&copy; <a href="http://osm.org/copyright">OpenStreetMap</a> contributors'
		}).addTo(map);
		var cams = [];
		var mypopup = L.popup(document.getElementById("MyPlayer_div"));
		<?php
			foreach ($myCams as $cam) {
				if ($cam->coordinates == "") continue;
		?>
				// add a marker in the given location, attach some popup content to it and open the popup
				cams[<?php echo "$cam->id"; ?>] = [<?php echo "$cam->coordinates"; ?>];
				var marker = L.marker([<?php echo "$cam->coordinates"; ?>]).addTo(map)
					.bindPopup(document.getElementById("MyPlayer_div"), {minWidth:"100px", maxWidth:"100px"});
				marker.on('click', function() {
					cam_id = <?php echo "$cam->id"; ?>;
					console.log(this);
				});
		<?php
			}
		?>
		console.log(cams);
		map.fitBounds(cams)
	});
	</script>

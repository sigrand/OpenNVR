<?php
/* @var $this CamsController */
?>
<link rel="stylesheet" href="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.css" />
<script src="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.js"></script>


<div style="display:none;">
<div id="MyPlayer_div" style="width:300px;height:168px;">
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
                <embed FlashVars="autoplay=0&playlist=1&buffer=0.3&show_buttons=true"
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
			<a href="" target="_blank" id="open_link">Открыть в новом окне</a>
</div>
</div>
<div class="col-sm-12" style="padding-left:5px;padding-right:5px;">

<div class="carousel slide" style="padding-bottom:5px;">
<div class="carousel-inner">
<div class="cols-sm-12 active">
	<?php
		for ($i=1; $i<=4; $i++) {
	?>
	<div class="col-sm-3 col-xs-6 carousel_players" style="height:174px;padding:0px">
                <object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" width="100%" height="100%" id="MyPlayer<?php echo "$i\""; ?>
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
                <embed FlashVars="autoplay=0&playlist=1&buffer=0.3&auto_horizontal_mode=true"
                src="<?php echo Yii::app()->request->baseUrl; ?>/player/MyPlayer_hi_lo.swf"
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
            </object>
	</div>
	<div id="on_MyPlayer<?php echo "$i\""; ?> num="<?php echo "$i\""; ?> style="position:absolute;padding:0;margin:0;"></div>
	<?php
		}
	?>

<?php
	foreach ($myCams as $cam) {
	}
?>
</div>
</div>
<style type="text/css">
.carousel-control {
  position: absolute;
  top: 50%;
  left: 15px;
  width: 44px;
  height: 44px;
  margin-top: -20px;
  font-size: 60px;
  font-weight: 100;
  line-height: 30px;
  color: #ffffff;
  text-align: center;
  background: #222222;
  border: 3px solid #ffffff;
  -webkit-border-radius: 23px;
  -moz-border-radius: 23px;
  border-radius: 23px;
  opacity: 0.5;
  filter: alpha(opacity=50);
}
.carousel-control.right {
  left: auto;
  right: 15px;
}
.carousel-control:hover {
  color: #ffffff;
  text-decoration: none;
  opacity: 0.9;
  filter: alpha(opacity=90);
}
</style>
	<a id="left_button" class="carousel-control left" href="#myCarousel" data-slide="prev">‹</a>
	<a id="right_button" class="carousel-control right" href="#myCarousel" data-slide="next">›</a>
</div>
				<div class="cols-sm-12" id="map_div" style="width:100%;height:1000px;"></div>
	</div>
	<script>

	var carousel_cams = [];
	var carousel_position = 0;
	var cam_id = "";
	var cams = [];
	var cams_markers = [];
	var markers = {};
	var m = [];
	var map;
	function flashInitialized() {
		if (cam_id != "") {
			// for popup player;
			document["MyPlayer"].setSource(<?php echo '"rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/"'; ?>, cam_id+"_low", cam_id);
			document.getElementById("open_link").href="/index.php/cams/fullscreen/full/1/id/"+cam_id;
			cam_id = "";
		} else {
			// for carousel players
			for (i=1;i<=4;i++) {
				document["MyPlayer"+i].setSource(<?php echo '"rtmp://'.Yii::app()->params['moment_server_ip'].':'.Yii::app()->params['moment_live_port'].'/live/"'; ?>, carousel_cams[i]+"_low", carousel_cams[i]);
			}
		}
	}

	$(document).ready(function(){
		var mypopup = L.popup(document.getElementById("MyPlayer_div"));
		<?php
			foreach ($myCams as $cam) {
		?>
				cams.push(<?php echo "\"$cam->id\""; ?>);
		<?php
				if ($cam->coordinates != "") {
		?>
				cams_markers.push(L.latLng(<?php echo "$cam->coordinates"; ?>));
				var marker = L.marker([<?php echo "$cam->coordinates"; ?>]);
				markers[<?php echo "\"$cam->id\""; ?>] = marker;
				m[<?php echo "\"$cam->id\""; ?>] = marker.on('click', function() {
					cam_id = <?php echo "$cam->id"; ?>;
				});
		<?php
				}
			}
		?>
		console.log(markers);

		$(window).on('load resize',function(){
			$(".carousel_players").css("height", Math.round($(".carousel_players").width()*9/16));
			for (ii=1;ii<=4;ii++) {
				$("#on_MyPlayer"+ii).offset($("#MyPlayer"+ii+" embed").offset());
				$("#on_MyPlayer"+ii).css("width", $("#MyPlayer"+ii+" embed").width());
				$("#on_MyPlayer"+ii).css("height", $("#MyPlayer"+ii+" embed").height());
			}
			$("#map_div").css("height", Math.round($(window).height() - $(".carousel_players").height() - $(".navbar").height()-17)+"px");
			if (!map) {
				map = L.map('map_div', {closePopupOnClick:false});
				L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
					attribution: '&copy; <a href="http://osm.org/copyright">OpenStreetMap</a> contributors'
				}).addTo(map);
				$.each(markers, function(key, val) {
					val.addTo(map).bindPopup(document.getElementById("MyPlayer_div"));
				});
			}
			map.fitBounds(L.latLngBounds(cams_markers));
		});

		for (i=1;i<=4;i++) {
			$("#on_MyPlayer"+i).click(function(){
				cam_id = carousel_cams[this.id.substring(11)];
				map.setView(markers[carousel_cams[this.id.substring(11)]].getLatLng(), 20, {animate:true});
				markers[carousel_cams[this.id.substring(11)]].openPopup();
			});
		}

/* carousel */
		var set_carousel_cams = function(carousel_position) {
			if (cams.length <= 4) {
				var i = 1;
				for (c=0; c < cams.length; c++) {
					carousel_cams[i] = cams[c];
					i++;
				}
				for (j=i;j<=4;j++) carousel_cams[j]=carousel_cams[i-1];
				return false;
			}
			if ((carousel_position >= 0) && (carousel_position <= (cams.length - 4))) {
				carousel_cams[1] = cams[carousel_position];
				carousel_cams[2] = cams[carousel_position+1];
				carousel_cams[3] = cams[carousel_position+2];
				carousel_cams[4] = cams[carousel_position+3];
				return true;
			} else {
				if (carousel_position < 0) {
					carousel_position = cams.length - carousel_position - 1;
					return set_carousel_cams(carousel_potition);
				} else {
					if (carousel_position > (cams.length - 4)) {
						if (carousel_position >= cams.length) {
							carousel_position = carousel_position % cams.length;
							return set_carousel_cams(carousel_position);
						} else {
							carousel_cams[1] = cams[carousel_position];
							carousel_cams[2] = cams[(carousel_position+1>=cams.length)?(carousel_position+1)%cams.length:carousel_position+1];
							carousel_cams[3] = cams[(carousel_position+2>=cams.length)?(carousel_position+2)%cams.length:carousel_position+2];
							carousel_cams[4] = cams[(carousel_position+3>=cams.length)?(carousel_position+3)%cams.length:carousel_position+3];
							return true;
						}
					} else {
						return false;
					}
				}
			}
		}
		set_carousel_cams(0);
		$("#left_button").click(function() {
			if (set_carousel_cams(--carousel_position))
			flashInitialized();
		});
		$("#right_button").click(function() {
			if (set_carousel_cams(++carousel_position))
			flashInitialized();
		});
	});
	</script>

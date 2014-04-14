<?php
/* @var $this CamsController */
?>
<link rel="stylesheet" href="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.css" />
<script src="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.js"></script>
<script src="/js/leaflet/leaflet.draw.js"></script>
<link rel="stylesheet" href="/css/leaflet.draw.css" />

<div class="col-sm-12">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo $model->isNewRecord ? Yii::t('cams', 'Fields marked with an * are required') : Yii::t('cams', 'Edit'); ?></h3>
		</div>
		<div class="panel-body">
			<?php $form = $this->beginWidget('CActiveForm', array(
				'action' => $model->isNewRecord ? $this->createUrl('cams/add') : $this->createUrl('cams/edit', array('id' => $model->getSessionId())),
				'id' => 'cams-form',
				'enableClientValidation' => true,
				'clientOptions' => array('validateOnSubmit' => true),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
				); ?>
				<div class="row">
					<div class="col-sm-4">
						<div id="MyPlayer_div">
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
						<a href="" target="_blank" id="open_link"><?php echo Yii::t('cams', 'open in new window'); ?></a>
					</div>
				</div>
				<div class="col-sm-8">

					<div class="form-group">
						<?php echo $form->labelEx($model, 'name', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php echo $form->textField($model, 'name', array('class' => 'form-control')); ?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'name'); ?>
						</div>
					</div>
					<div class="form-group">
						<?php echo $form->labelEx($model, 'desc', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php echo $form->textArea($model, 'desc', array('class' => 'form-control')); ?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'desc'); ?>
						</div>
					</div>
					<div class="form-group">
						<?php echo $form->labelEx($model, 'url', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php echo $form->textField($model, 'url', array('class' => 'form-control')); ?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'url'); ?>
						</div>
					</div>
					<div class="form-group">
						<?php echo $form->labelEx($model, 'prev_url', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php echo $form->textField($model, 'prev_url', array('class' => 'form-control')); ?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'prev_url'); ?>
						</div>
					</div>
					<div class="form-group">
						<?php echo $form->labelEx($model, 'time_offset', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php
							Yii::import('ext.timezones.index', 1);
							$tz = new timezones;
							echo $form->dropDownList($model, 'time_offset', $tz->getTimezones(), array('class' => 'form-control'));
							?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'time_offset'); ?>
						</div>
					</div>		
					<div class="form-group">
						<?php echo $form->labelEx($model, 'server_id', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php echo $form->dropDownList($model, 'server_id', $servers, array('class' => 'form-control')); ?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'server_id'); ?>
						</div>
					</div>
					<div class="form-group">
						<?php echo $form->labelEx($model, 'coordinates', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php echo $form->textField($model, 'coordinates', array('class' => 'form-control')); ?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'coordinates'); ?>
						</div>
					</div>
					<div class="form-group">
						<?php echo $form->labelEx($model, 'view_area', array('class' => 'col-sm-4 control-label')); ?>
						<div class="col-sm-8">
							<?php echo $form->textField($model, 'view_area', array('class' => 'form-control')); ?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, 'view_area'); ?>
						</div>
					</div>
				</div>
			</div>
			<div class="col-sm-12">
				<div class="form-group">
					<div id="map" style="height:400px;">
					</div>
				</div>
				<div class="form-group">
					<div class="col-sm-offset-2 col-sm-10">
						<div class="checkbox">
							<label>
								<?php echo $form->checkBox($model, 'show'); ?>
								<?php echo $form->labelEx($model, 'show'); ?>
							</label>
						</div>
					</div>
				</div>
				<div class="form-group">
					<div class="col-sm-offset-2 col-sm-10">
						<div class="checkbox">
							<label>
								<?php echo $form->checkBox($model, 'record'); ?>
								<?php echo $form->labelEx($model, 'record'); ?>
							</label>
						</div>
					</div>
				</div>
				<div class="form-group">
					<div class="col-sm-offset-2 col-sm-10">
						<?php echo CHtml::submitButton($model->isNewRecord ? Yii::t('cams', 'Add') : Yii::t('cams', 'Save'), array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php $this->endWidget(); ?>
			</div>

		</div>

	</div>
</div>
<?php
if($model->server_id) {
	$server = Servers::model()->findByPK($model->server_id);
}
if(Yii::app()->user->hasFlash('notify')) {
	$notify = Yii::app()->user->getFlash('notify');
	?>
	<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.
		<?php
		echo Yii::t('cams',
			'Go to {user CP}',
			array(
				'{user CP}' => CHtml::link(
					Yii::t('cams', 'personal cabinet'),
					$this->createUrl('cams/manage')
					)
				)
			);
			?>
			?
		</div>
		<?php } ?>
	</div>

	<script>
	var cam_marker = undefined, cam_view_area = undefined;
	var map, marker, polygon;

	function flashInitialized() {
		var cam_id_low = "<?php echo $model->getSessionId(true); ?>";
		var cam_id = "<?php echo $model->getSessionId(true); ?>";
		document['MyPlayer'].setSource("<?php if(isset($server)) { echo 'rtmp://'.$server->ip.':'.$server->l_port.'/live/'; } ?>", cam_id_low, cam_id);
		document.getElementById("open_link").href="<?php echo $this->createUrl('cams/fullscreen', array('full' => 1, 'id' => $model->getSessionId())); ?>";
	}

	$(document).ready(function(){
		$('#MyPlayer_div embed').height($('#MyPlayer_div').width()*9/16);
		$('#MyPlayer_div').css('padding-top', $('#MyPlayer_div').parent().parent().height() - $('#MyPlayer_div embed').height());
		map = L.map('map').setView([<?php if ($model->coordinates == "") echo "0,0"; else echo "$model->coordinates"; ?>], 18);
		L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
			attribution: '&copy; <a href="http://osm.org/copyright">OpenStreetMap</a> contributors'
		}).addTo(map);
		var popup = L.popup();
		var LeafIcon = L.Icon.extend({
			options: {
				shadowUrl: '/images/shadow.png',
				iconSize:     [40, 41],
				shadowSize:   [51, 37],
				iconAnchor:   [17, 33],
				shadowAnchor: [17, 30],
				popupAnchor:  [0, -10]
			}
		});
		var cam_icon = new LeafIcon({iconUrl: '/images/cam_icon.png'});
		marker = L.marker([<?php echo $model->coordinates == "" ? "0,0" : "$model->coordinates"; ?>], {icon:cam_icon}).addTo(map);
		polygon = L.polygon([<?php echo $model->view_area == "" ? "" : "$model->view_area"; ?>], {color:'#2f85cb'}).addTo(map);

		// Initialise the FeatureGroup to store editable layers
		var drawnItems = new L.FeatureGroup();
		map.addLayer(drawnItems);

		// Initialise the draw control and pass it the FeatureGroup of editable layers
		var drawControl = new L.Control.Draw({
			edit: false,
			draw: {
				polyline: false,
				polygon: {
					allowIntersection: false,
					showArea: true,
					drawError: {
						color: '#b00b00',
						timeout: 1000
					},
					shapeOptions: {
						color: '#2f85cb'
					}
				},
				circle: false,
				rectangle: false,
				marker: {
					icon: cam_icon
				}
			}
		});
		map.addControl(drawControl);
		var cam_marker_click = undefined, cam_view_area_click = undefined;
		map.on('draw:created', function (e) {
			var type = e.layerType,
			layer = e.layer;

			if (type === 'marker') {
				if (cam_marker !== undefined) {
					map.removeLayer(cam_marker.layer);
				}
				cam_marker = e;
				drawnItems.addLayer(layer);
				document.getElementById("Cams_coordinates").value = cam_marker.layer.getLatLng().lat+", "+cam_marker.layer.getLatLng().lng;
				if (marker !== undefined) map.removeLayer(marker);
			} else {
				if (cam_view_area !== undefined) {
					map.removeLayer(cam_view_area.layer);
				}
				cam_view_area = e;
				drawnItems.addLayer(layer);
				var view_area_points = "";
				$.each(cam_view_area.layer._latlngs, function(key, val) {
					view_area_points += "[" + val.lat + "," + val.lng + "],";
				});
				view_area_points = view_area_points.substring(0, view_area_points.length - 1);
				document.getElementById("Cams_view_area").value = "["+view_area_points+"]";
				if (polygon !== undefined) map.removeLayer(polygon);
			}
			console.debug(e);

		});

		map.on('draw:edited', function (e) {
			var layers = e.layers;
			var countOfEditedLayers = 0;
			layers.eachLayer(function(layer) {
				countOfEditedLayers++;
			});
			console.log("Edited " + countOfEditedLayers + " layers");
		});


		function onMapClick(e) {
			marker.setLatLng(e.latlng);
			marker.update();
			document.getElementById("Cams_coordinates").value = e.latlng.lat+", "+e.latlng.lng;
		}
	});
</script>

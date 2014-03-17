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
				'action' => $model->isNewRecord ? $this->createUrl('cams/add') : $this->createUrl('cams/edit', array('id' => $model->id)),
				'id' => 'cams-form',
				'enableClientValidation' => true,
				'clientOptions' => array('validateOnSubmit' => true),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
				); ?>
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
						<?php echo $form->textField($model, 'time_offset', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'time_offset'); ?>
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
				<div class="form-group">
					<div class="col-sm-16" id="map" style="height:400px;">
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
		<?php
		if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>. <?php echo Yii::t('cams', 'Go to {user CP}', array('{user CP}')); ?> <?php echo CHtml::link(Yii::t('cams', 'personal cabinet'), $this->createUrl('cams/manage')); ?>?</div>
		<?php } ?>
	</div>


	<script>
	var cam_marker = undefined, cam_view_area = undefined;
	var map, marker, polygon;
	$(document).ready(function(){
		map = L.map('map').setView([<?php if ($model->coordinates == "") echo "0,0"; else echo "$model->coordinates"; ?>], 15);
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
		marker = L.marker([<?php if ($model->coordinates == "") echo "0,0"; else echo "$model->coordinates"; ?>], {icon:cam_icon}).addTo(map);
		polygon = L.polygon([<?php if ($model->view_area == "") echo ""; else echo "$model->view_area"; ?>], {color:'#2f85cb'}).addTo(map);

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

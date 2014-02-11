<?php
/* @var $this CamsController */
?>
<link rel="stylesheet" href="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.css" />
<script src="http://cdn.leafletjs.com/leaflet-0.7.2/leaflet.js"></script>

<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo $model->isNewRecord ? 'Заполните поля. * - помечены обязательные' : 'Редактирование'; ?></h3>
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
					<div class="col-sm-16" id="map" style="height:200px;">
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
						<?php echo CHtml::submitButton($model->isNewRecord ? 'Добавить' : 'Сохранить', array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php $this->endWidget(); ?>
			</div>
		</div>
		<?php
		if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>. Перейти в <?php echo CHtml::link('личный кабинет', $this->createUrl('cams/manage')); ?>?</div>
		<?php } ?>
	</div>


	<script>
	$(document).ready(function(){
		var map = L.map('map').setView([<?php if ($model->coordinates == "") echo "0,0"; else echo "$model->coordinates"; ?>], 15);
		L.tileLayer('http://{s}.tile.osm.org/{z}/{x}/{y}.png', {
			attribution: '&copy; <a href="http://osm.org/copyright">OpenStreetMap</a> contributors'
		}).addTo(map);
		var popup = L.popup();
		var marker = L.marker([<?php if ($model->coordinates == "") echo "0,0"; else echo "$model->coordinates"; ?>]).addTo(map);
		function onMapClick(e) {
			marker.setLatLng(e.latlng);
			marker.update();
			document.getElementById("Cams_coordinates").value = e.latlng.lat+", "+e.latlng.lng;
		}
		map.on('click', onMapClick);
	});
	</script>

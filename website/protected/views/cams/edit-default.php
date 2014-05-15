<div class="col-md-offset-3 col-md-5">
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
					<?php echo $form->labelEx($model, 'user', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'user', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'user'); ?>
					</div>
				</div>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'pass', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'pass', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'pass'); ?>
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
			<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>. <?php echo Yii::t('cams', 'Go to {user CP}', array('{user CP}' => CHtml::link(Yii::t('cams', 'personal cabinet'), $this->createUrl('cams/manage')))); ?>?</div>
			<?php } ?>
		</div>
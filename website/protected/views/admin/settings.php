<div class="col-sm-offset-3 col-sm-6">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo  Yii::t('admin', 'App settings'); ?></h3>
			</div>
			<div class="panel-body">
				<?php
				$form = $this->beginWidget('CActiveForm', array(
					'action' => $this->createUrl('admin/settings'),
					'id' => 'settings-form',
					'enableClientValidation' => true,
					'clientOptions' => array('validateOnSubmit' => true),
					'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
					)
				);
				foreach ($models as $k => $model) {
					?>
					<div class="form-group">
						<label class="col-sm-4 control-label" for="Settings_option"><?php echo $model->option; ?></label>
						<div class="col-sm-8">
							<?php echo $form->hiddenField($model, '['.$k.']id'); ?>
							<?php
							if($model->option == 'style') {
								echo $form->dropDownList($model, '['.$k.']value', Settings::model()->getStyles(), array('class' => 'form-control'));
							} elseif ($model->option == 'index') {
								echo $form->dropDownList($model, '['.$k.']value', array('map' => 'map', 'list' => 'list'), array('class' => 'form-control'));
							} else {
								echo $form->textField($model, '['.$k.']value', array('class' => 'form-control'));
							}
							?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model, '['.$k.']value'); ?>
						</div>
					</div>
					<?php
				}
				?>
				<div class="form-group">
					<div class="col-sm-offset-4 col-sm-2">
						<?php echo CHtml::submitButton(Yii::t('admin', 'Save'), array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php $this->endWidget(); ?>
			</div>
		</div>
	</div>
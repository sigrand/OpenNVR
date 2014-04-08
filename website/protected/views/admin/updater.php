<div class="col-sm-offset-3 col-sm-6">
	<?php
	if(Yii::app()->user->hasFlash('versions')) {
		$v = json_decode(Yii::app()->user->getFlash('versions'), 1);
		}
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title"><?php echo  Yii::t('admin', 'Updater'); ?></h3>
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
					$status = isset($v[$model->option]) && $v[$model->option] > $model->value ? 1 : 0;
					?>
					<div class="form-group">
						<label class="col-sm-3 control-label" for="Settings_option"><?php echo $model->option; ?></label>
						<div class="col-sm-3">
							<?php echo $form->hiddenField($model, '['.$k.']id'); ?>
							<?php echo $form->textField($model, '['.$k.']value', array('class' => 'form-control', 'readOnly' => 1)); ?>
						</div>
						<div class="col-sm-3">
							<?php
							if(isset($v[$model->option])) {
								echo CHtml::textField('last_'.$model->option, $v[$model->option], array('class' => 'form-control', 'readOnly' => 1));
							}
							?>
						</div>
						<div class="col-sm-3">
						<?php echo CHtml::link(Yii::t('admin', 'update'), $this->createUrl('admin/update'.ucfirst($model->option)), array('class' => 'btn btn-primary '.($status ? '' : 'disabled'))); ?>
						</div>

					</div>
					<?php
				}
				?>
				<div class="form-group">
					<div class="col-sm-offset-4 col-sm-2">
						<?php echo CHtml::link(Yii::t('admin', 'Check for updates'), $this->createUrl('admin/checkUpdate'), array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php $this->endWidget(); ?>
			</div>
		</div>
	</div>
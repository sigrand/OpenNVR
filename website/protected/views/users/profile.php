<?php
/* @var $this UsersController */
Yii::app()->clientScript->registerCssFile(Yii::app()->baseUrl.'/css/passfield.min.css');
Yii::app()->clientScript->registerScriptFile(Yii::app()->baseUrl.'/js/passfield.min.js');
?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('user', 'Изменение часового пояса'); ?></h3>
		</div>
		<div class="panel-body">
			<?php $form=$this->beginWidget('CActiveForm', array(
				'action' => $this->createUrl('profile', array('id' => 'time_offset')),
				'id' => 'profile-form',
				'enableClientValidation' => true,
				'clientOptions' => array('validateOnSubmit' => true),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
				); ?>
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
					<?php echo $form->labelEx($model,'options', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php
						echo $form->radioButtonList(
							$model,
							'options',
							array(
								0 => Yii::t('user', 'пользователя'),
								1 => Yii::t('user', 'камеры'),
								)
							);
							?>
						</div>
						<div class="col-sm-offset-4 col-sm-8">
							<?php echo $form->error($model,'options'); ?>
						</div>
					</div>
					<div class="form-group">
						<div class="col-sm-offset-2 col-sm-10">
							<?php echo CHtml::submitButton(Yii::t('user', 'Изменить'), array('class' => 'btn btn-primary')); ?>
						</div>
					</div>
					<?php $this->endWidget(); ?>
					<?php
					if(Yii::app()->user->hasFlash('notifyTO')) {
						$notify = Yii::app()->user->getFlash('notifyTO');
						?>
						<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
						<?php } ?>
					</div>
				</div>
			</div><div class="col-md-offset-3 col-md-5">
			<div class="panel panel-default">
				<div class="panel-heading">
					<h3 class="panel-title"><?php echo Yii::t('user', 'Изменение пароля'); ?></h3>
				</div>
				<div class="panel-body">
					<?php $form=$this->beginWidget('CActiveForm', array(
						'action' => $this->createUrl('profile', array('id' => 'pass')),
						'id' => 'profile-form',
						'enableClientValidation' => true,
						'clientOptions' => array('validateOnSubmit' => true),
						'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
						)
						); ?>
						<div class="form-group">
							<?php echo $form->labelEx($model, 'old_pass', array('class' => 'col-sm-4 control-label')); ?>
							<div class="col-sm-8">
								<?php echo $form->textField($model, 'old_pass', array('class' => 'form-control pass')); ?>
							</div>
							<div class="col-sm-offset-4 col-sm-8">
								<?php echo $form->error($model, 'old_pass'); ?>
							</div>
						</div>
						<div class="form-group">
							<?php echo $form->labelEx($model, 'new_pass', array('class' => 'col-sm-4 control-label')); ?>
							<div class="col-sm-8">
								<?php echo $form->textField($model, 'new_pass', array('class' => 'form-control pass')); ?>
							</div>
							<div class="col-sm-offset-4 col-sm-8">
								<?php echo $form->error($model, 'new_pass'); ?>
							</div>
						</div>
						<div class="form-group">
							<div class="col-sm-offset-2 col-sm-10">
								<?php echo CHtml::submitButton(Yii::t('user', 'Изменить'), array('class' => 'btn btn-primary')); ?>
							</div>
						</div>
						<?php $this->endWidget(); ?>
						<?php
						if(Yii::app()->user->hasFlash('notify')) {
							$notify = Yii::app()->user->getFlash('notify');
							?>
							<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
							<?php } ?>
						</div>
					</div>
				</div>
				<script type="text/javascript">
				$(".pass").passField({
					showGenerate: true,
					showWarn: true,
					showTip: true
				});
				</script>
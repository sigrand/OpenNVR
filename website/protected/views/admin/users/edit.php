<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('admin', 'Fields marked with an * are required'); ?></h3>
		</div>
		<div class="panel-body">
			<?php $form = $this->beginWidget('CActiveForm', array(
				'action' => $this->createUrl('admin/addUser'),
				'id' => 'admin-form',
				'enableClientValidation' => true,
				'clientOptions' => array('validateOnSubmit' => true),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
				); ?>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'nick', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'nick', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'nick'); ?>
					</div>
				</div>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'email', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'email', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'email'); ?>
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
				<?php if(Yii::app()->user->permissions == 3) { ?>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'rang', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->dropDownList($model, 'rang', array('1' => Yii::t('admin', 'viewer'), '2' => Yii::t('admin', 'operator'), '3' => Yii::t('admin', 'admin')), array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'rang'); ?>
					</div>
				</div>
				<?php } ?>
				<div class="form-group">
					<div class="col-sm-offset-2 col-sm-10">
						<?php echo CHtml::submitButton(Yii::t('admin', 'Add'), array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php $this->endWidget(); ?>
			</div>
		</div>
		<?php
		if(Yii::app()->user->hasFlash('notify')) {
			$notify = Yii::app()->user->getFlash('notify');
			?>
			<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?></div>
			<?php } ?>
		</div>
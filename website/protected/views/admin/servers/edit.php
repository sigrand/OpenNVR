<?php
/* @var $this AdminController */
?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo $model->isNewRecord ? Yii::t('admin', 'Servers form') : Yii::t('admin', 'Edit'); ?></h3>
		</div>
		<div class="panel-body">
			<?php $form = $this->beginWidget('CActiveForm', array(
				'action' => $model->isNewRecord ? $this->createUrl('admin/serverAdd') : $this->createUrl('admin/serverEdit', array('id' => $model->id)),
				'id' => 'servers-form',
				'enableClientValidation' => true,
				'clientOptions' => array('validateOnSubmit' => true),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
				); ?>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'comment', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'comment', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'comment'); ?>
					</div>
				</div>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'protocol', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->dropDownList($model, 'protocol', array('http' => 'http', 'https' => 'https'), array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'protocol'); ?>
					</div>
				</div>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'ip', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'ip', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'ip'); ?>
					</div>
				</div>
				<div class="form-group">
					<?php echo $form->labelEx($model, 's_port', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 's_port', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 's_port'); ?>
					</div>
				</div>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'w_port', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'w_port', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'w_port'); ?>
					</div>
				</div>				<div class="form-group">
					<?php echo $form->labelEx($model, 'l_port', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'l_port', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'l_port'); ?>
					</div>
				</div>
				
				<div class="form-group">
					<div class="col-sm-offset-2 col-sm-10">
						<?php echo CHtml::submitButton($model->isNewRecord ? Yii::t('admin', 'Add') : Yii::t('admin', 'Save'), array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php $this->endWidget(); ?>
			</div>
		</div>
		<?php //TODO edit shit below
		if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>. <?php echo Yii::t('admin', 'Go to {user CP}', array('{user CP}' => CHtml::link(Yii::t('admin', 'Servers manage'), $this->createUrl('admin/servers')))); ?>?</div>
		<?php } ?>
	</div>


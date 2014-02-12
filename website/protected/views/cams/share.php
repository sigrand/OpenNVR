<?php
/* @var $this CamsController */
?>
<div class="col-md-offset-2 col-md-7">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><?php echo Yii::t('cams', 'Выберите кому камеры шарить'); ?></h3>
		</div>
		<div class="panel-body">
			<?php
			$form = $this->beginWidget('CActiveForm', array(
				'action' => $this->createUrl('cams/share'),
				'id' => 'share-form',
				'enableClientValidation' => true,
				'clientOptions' => array('validateOnSubmit' => true),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
				);
				?>
				<div class="form-group">
					<?php echo $form->hiddenField($model, 'hcams'); ?>
					<?php echo $form->labelEx($model, 'cams', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textField($model, 'cams', array('readOnly' => '1', 'class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'cams'); ?>
					</div>
				</div>
				<div class="form-group">
					<?php echo $form->labelEx($model, 'emails', array('class' => 'col-sm-4 control-label')); ?>
					<div class="col-sm-8">
						<?php echo $form->textArea($model, 'emails', array('class' => 'form-control')); ?>
					</div>
					<div class="col-sm-offset-4 col-sm-8">
						<?php echo $form->error($model, 'emails'); ?>
					</div>
				</div>
				<div class="form-group">
					<div class="col-sm-offset-2 col-sm-10">
						<?php echo CHtml::submitButton(Yii::t('cams', 'Расшарить'), array('class' => 'btn btn-primary')); ?>
					</div>
				</div>
				<?php $this->endWidget(); ?>
			</div>
		</div>
	</div>


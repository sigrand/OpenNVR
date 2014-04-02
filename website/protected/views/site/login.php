<?php
Yii::app()->clientScript->registerCssFile(Yii::app()->baseUrl.'/css/passfield.min.css');
Yii::app()->clientScript->registerScriptFile(Yii::app()->baseUrl.'/js/passfield.min.js');
?>
<div class="col-md-offset-3 col-md-5">
	<div class="panel panel-default">
		<div class="panel-heading">
			<h3 class="panel-title"><? echo Yii::t('register', 'Login form');?></h3>
		</div>
		<div class="panel-body">
			<?php
			$form = $this->beginWidget('CActiveForm', array(
				'action' => $this->createUrl('site/login', array()),
				'id' => 'login-form',
				'enableClientValidation' => false,
				'clientOptions' => array(
					'validateOnSubmit' => true,
					),
				'htmlOptions' => array('class' => 'form-horizontal', 'role' => 'form')
				)
			);
			?>
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
					<?php echo $form->passwordField($model, 'pass', array('id' => 'passfield', 'class' => 'form-control')); ?>
				</div>
				<div class="col-sm-offset-4 col-sm-8">
				<?php echo $form->error($model, 'pass'); ?>
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-2 col-sm-10">
					<div class="checkbox">
						<label>
							<?php echo $form->checkBox($model, 'rememberMe'); ?>
							<?php echo $form->labelEx($model, 'rememberMe'); ?>
						</label>
					</div>
				</div>
			</div>
			<div class="form-group">
				<div class="col-sm-offset-2 col-sm-10">
					<?php echo CHtml::submitButton(Yii::t('register', 'Login'), array('class' => 'btn btn-primary')); ?>
				</div>
			</div>
			<a class="links" href="<?php echo $this->createUrl('site/register'); ?>"><?php echo Yii::t('register', 'Registration');?></a><br/>
			<a class="links" href="<?php echo $this->createUrl('site/recovery'); ?>"><?php echo Yii::t('register', 'Password recovery');?></a>
			<?php $this->endWidget(); ?>
		</div>
	</div>
</div>
<script type="text/javascript">
$("#passfield").passField({
	showGenerate: false,
	showWarn: false,
	showTip: false
});
</script>
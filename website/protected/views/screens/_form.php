<?php
/* @var $this ScreensController */
/* @var $model Screens */
/* @var $form CActiveForm */
?>


<div class="col-sm-12">
	<?php
	if(Yii::app()->user->hasFlash('notify')) {
		$notify = Yii::app()->user->getFlash('notify');
		?>
		<div class="alert alert-<?php echo $notify['type']; ?>"><?php echo $notify['message']; ?>.</div>
		<?php } ?>
		<div class="panel panel-default">
			<div class="panel-heading">
				<h3 class="panel-title">
					<?php echo $model->isNewRecord ? Yii::t('screens', 'New screen') : Yii::t('screens', 'Edit screen'); ?>
				</h3>
			</div>
			<div class="panel-body">

<div class="form">

<?php $form=$this->beginWidget('CActiveForm', array(
	'id'=>'screens-form',
	'enableAjaxValidation'=>false,
)); ?>

<?php if (($model->isNewRecord) && ($type == 'custom')) { ?>
	<div id="new_screen" style="display:none"></div>
<?php } ?>

<div class="form-group">
	<label class="col-sm-2 control-label required" for="ProfileForm_time_offset"><?php echo Yii::t('screens', 'Screen name'); ?></label>
	<div class="col-sm-10">
		<?php echo $form->textField($model,'name',array('size'=>60,'maxlength'=>100, 'class'=>'form-control')); ?>
	</div>
</div>
<div class="form-group">
	<div class="col-md-2" style="padding-top:10px;padding-bottom:10px;">
		<a id="add_cam_a" href="#"><img id="add_cam_div" width="32px" src="<?php echo Yii::app()->baseUrl; ?>/images/add-icon.png" alt="<?php echo Yii::t('screens', 'Add cam'); ?>"><?php echo Yii::t('screens', 'Add cam'); ?></a>
	</div>
	<div class="col-md-2"  style="padding-top:10px;padding-bottom:10px;">
		<?php echo CHtml::submitButton(Yii::t('screens', 'Save'), array('class' => 'btn btn-primary')); ?>
	</div>

</div>

	<div id="screen" class="col-md-8">
<?php
	// max 16 cams on screen
	$colors = array(
		1 => "#FF0000",
		2 => "#00FF00",
		3 => "#0000FF",
		4 => "#000000",
		5 => "#FFFF00",
		6 => "#FF00FF",
		7 => "#00FFFF",
		8 => "#080000",
		9 => "#000800",
		10 => "#000008",
		11 => "#800000",
		12 => "#008000",
		13 => "#000080",
		14 => "#008080",
		15 => "#800080",
		16 => "#808000",
	);

	for ($i=1; $i <= 16; $i++) {
	echo '<div class="cams" id="cam'.$i.'_div" style="display:none; background-color:'.$colors[$i].';">';
		echo '<div title="'.Yii::t('screens', 'Delete').'" class="remove_cam_button" style="background-image:url('.Yii::app()->baseUrl.'/images/remove_button.png);';
		echo 'width:16px; height:16px;right:0px;position:absolute"></div>';
		echo Yii::t('screens', 'Cam').$form->dropDownList($model,'cam'.$i.'_id', $myCams, array('class'=>'form-control'));
		echo Yii::t('screens', 'Descr').$form->textField($model,'cam'.$i.'_descr',array('size'=>60,'maxlength'=>100, 'class'=>'form-control'));
		echo '<div style="display:none;">';
			echo $form->textField($model,'cam'.$i.'_x');
			echo $form->textField($model,'cam'.$i.'_y');
			echo $form->textField($model,'cam'.$i.'_w');
			echo $form->textField($model,'cam'.$i.'_h');
		echo '</div>';
	echo '</div>';
	}
?>
	</div>

<?php $this->endWidget(); ?>

</div><!-- form -->


			</div>
		</div>
</div>

	


<?php
	$baseUrl = Yii::app()->baseUrl;
	$cs = Yii::app()->getClientScript();
	$cs->registerScriptFile($baseUrl.'/js/screen_add_edit_form.js');
	$cs->registerCSSFile($baseUrl.'/css/screen_add_edit_form.css');
	$cs->registerCSSFile($baseUrl.'/css/jquery-ui-1.10.4.custom.min.css');
	$cs->registerScriptFile($baseUrl.'/js/jquery-ui-1.10.4.custom.min.js');
?>
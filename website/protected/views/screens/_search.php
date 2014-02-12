<?php
/* @var $this ScreensController */
/* @var $model Screens */
/* @var $form CActiveForm */
?>

<div class="wide form">

<?php $form=$this->beginWidget('CActiveForm', array(
	'action'=>Yii::app()->createUrl($this->route),
	'method'=>'get',
)); ?>

	<div class="row">
		<?php echo $form->label($model,'id'); ?>
		<?php echo $form->textField($model,'id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'name'); ?>
		<?php echo $form->textField($model,'name',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'owner_id'); ?>
		<?php echo $form->textField($model,'owner_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam1_id'); ?>
		<?php echo $form->textField($model,'cam1_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam1_x'); ?>
		<?php echo $form->textField($model,'cam1_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam1_y'); ?>
		<?php echo $form->textField($model,'cam1_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam1_w'); ?>
		<?php echo $form->textField($model,'cam1_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam1_h'); ?>
		<?php echo $form->textField($model,'cam1_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam1_descr'); ?>
		<?php echo $form->textField($model,'cam1_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam2_id'); ?>
		<?php echo $form->textField($model,'cam2_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam2_x'); ?>
		<?php echo $form->textField($model,'cam2_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam2_y'); ?>
		<?php echo $form->textField($model,'cam2_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam2_w'); ?>
		<?php echo $form->textField($model,'cam2_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam2_h'); ?>
		<?php echo $form->textField($model,'cam2_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam2_descr'); ?>
		<?php echo $form->textField($model,'cam2_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam3_id'); ?>
		<?php echo $form->textField($model,'cam3_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam3_x'); ?>
		<?php echo $form->textField($model,'cam3_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam3_y'); ?>
		<?php echo $form->textField($model,'cam3_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam3_w'); ?>
		<?php echo $form->textField($model,'cam3_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam3_h'); ?>
		<?php echo $form->textField($model,'cam3_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam3_descr'); ?>
		<?php echo $form->textField($model,'cam3_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam4_id'); ?>
		<?php echo $form->textField($model,'cam4_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam4_x'); ?>
		<?php echo $form->textField($model,'cam4_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam4_y'); ?>
		<?php echo $form->textField($model,'cam4_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam4_w'); ?>
		<?php echo $form->textField($model,'cam4_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam4_h'); ?>
		<?php echo $form->textField($model,'cam4_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam4_descr'); ?>
		<?php echo $form->textField($model,'cam4_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam5_id'); ?>
		<?php echo $form->textField($model,'cam5_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam5_x'); ?>
		<?php echo $form->textField($model,'cam5_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam5_y'); ?>
		<?php echo $form->textField($model,'cam5_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam5_w'); ?>
		<?php echo $form->textField($model,'cam5_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam5_h'); ?>
		<?php echo $form->textField($model,'cam5_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam5_descr'); ?>
		<?php echo $form->textField($model,'cam5_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam6_id'); ?>
		<?php echo $form->textField($model,'cam6_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam6_x'); ?>
		<?php echo $form->textField($model,'cam6_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam6_y'); ?>
		<?php echo $form->textField($model,'cam6_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam6_w'); ?>
		<?php echo $form->textField($model,'cam6_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam6_h'); ?>
		<?php echo $form->textField($model,'cam6_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam6_descr'); ?>
		<?php echo $form->textField($model,'cam6_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam7_id'); ?>
		<?php echo $form->textField($model,'cam7_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam7_x'); ?>
		<?php echo $form->textField($model,'cam7_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam7_y'); ?>
		<?php echo $form->textField($model,'cam7_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam7_w'); ?>
		<?php echo $form->textField($model,'cam7_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam7_h'); ?>
		<?php echo $form->textField($model,'cam7_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam7_descr'); ?>
		<?php echo $form->textField($model,'cam7_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam8_id'); ?>
		<?php echo $form->textField($model,'cam8_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam8_x'); ?>
		<?php echo $form->textField($model,'cam8_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam8_y'); ?>
		<?php echo $form->textField($model,'cam8_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam8_w'); ?>
		<?php echo $form->textField($model,'cam8_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam8_h'); ?>
		<?php echo $form->textField($model,'cam8_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam8_descr'); ?>
		<?php echo $form->textField($model,'cam8_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam9_id'); ?>
		<?php echo $form->textField($model,'cam9_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam9_x'); ?>
		<?php echo $form->textField($model,'cam9_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam9_y'); ?>
		<?php echo $form->textField($model,'cam9_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam9_w'); ?>
		<?php echo $form->textField($model,'cam9_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam9_h'); ?>
		<?php echo $form->textField($model,'cam9_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam9_descr'); ?>
		<?php echo $form->textField($model,'cam9_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam10_id'); ?>
		<?php echo $form->textField($model,'cam10_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam10_x'); ?>
		<?php echo $form->textField($model,'cam10_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam10_y'); ?>
		<?php echo $form->textField($model,'cam10_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam10_w'); ?>
		<?php echo $form->textField($model,'cam10_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam10_h'); ?>
		<?php echo $form->textField($model,'cam10_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam10_descr'); ?>
		<?php echo $form->textField($model,'cam10_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam11_id'); ?>
		<?php echo $form->textField($model,'cam11_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam11_x'); ?>
		<?php echo $form->textField($model,'cam11_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam11_y'); ?>
		<?php echo $form->textField($model,'cam11_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam11_w'); ?>
		<?php echo $form->textField($model,'cam11_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam11_h'); ?>
		<?php echo $form->textField($model,'cam11_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam11_descr'); ?>
		<?php echo $form->textField($model,'cam11_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam12_id'); ?>
		<?php echo $form->textField($model,'cam12_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam12_x'); ?>
		<?php echo $form->textField($model,'cam12_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam12_y'); ?>
		<?php echo $form->textField($model,'cam12_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam12_w'); ?>
		<?php echo $form->textField($model,'cam12_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam12_h'); ?>
		<?php echo $form->textField($model,'cam12_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam12_descr'); ?>
		<?php echo $form->textField($model,'cam12_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam13_id'); ?>
		<?php echo $form->textField($model,'cam13_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam13_x'); ?>
		<?php echo $form->textField($model,'cam13_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam13_y'); ?>
		<?php echo $form->textField($model,'cam13_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam13_w'); ?>
		<?php echo $form->textField($model,'cam13_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam13_h'); ?>
		<?php echo $form->textField($model,'cam13_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam13_descr'); ?>
		<?php echo $form->textField($model,'cam13_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam14_id'); ?>
		<?php echo $form->textField($model,'cam14_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam14_x'); ?>
		<?php echo $form->textField($model,'cam14_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam14_y'); ?>
		<?php echo $form->textField($model,'cam14_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam14_w'); ?>
		<?php echo $form->textField($model,'cam14_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam14_h'); ?>
		<?php echo $form->textField($model,'cam14_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam14_descr'); ?>
		<?php echo $form->textField($model,'cam14_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam15_id'); ?>
		<?php echo $form->textField($model,'cam15_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam15_x'); ?>
		<?php echo $form->textField($model,'cam15_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam15_y'); ?>
		<?php echo $form->textField($model,'cam15_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam15_w'); ?>
		<?php echo $form->textField($model,'cam15_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam15_h'); ?>
		<?php echo $form->textField($model,'cam15_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam15_descr'); ?>
		<?php echo $form->textField($model,'cam15_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam16_id'); ?>
		<?php echo $form->textField($model,'cam16_id'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam16_x'); ?>
		<?php echo $form->textField($model,'cam16_x'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam16_y'); ?>
		<?php echo $form->textField($model,'cam16_y'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam16_w'); ?>
		<?php echo $form->textField($model,'cam16_w'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam16_h'); ?>
		<?php echo $form->textField($model,'cam16_h'); ?>
	</div>

	<div class="row">
		<?php echo $form->label($model,'cam16_descr'); ?>
		<?php echo $form->textField($model,'cam16_descr',array('size'=>60,'maxlength'=>100)); ?>
	</div>

	<div class="row buttons">
		<?php echo CHtml::submitButton('Search'); ?>
	</div>

<?php $this->endWidget(); ?>

</div><!-- search-form -->